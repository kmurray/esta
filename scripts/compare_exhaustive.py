#!/usr/bin/env python
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from itertools import product, cycle
from bisect import bisect_left
from collections import OrderedDict
import sys
import pudb
import csv
import math

class TransitionScenario:
    def __init__(self, input_transitions, output_transition, delay, exact_prob, measured_prob):
        self.input_transitions = input_transitions
        self.output_transition = output_transition
        self.delay = delay
        self.exact_prob = exact_prob
        self.measured_prob = measured_prob

def main():
    args = parse_args()

    print "Loading ", args.reference_csv, "..."
    reference_data = load_csv(args.reference_csv)

    safely_pessimistic = False

    comparison_data = None
    if args.comparison_csv:
        print "Loading ", args.comparison_csv, "..."
        comparison_data = load_csv(args.comparison_csv)

        print "Comparing ..."
        safely_pessimistic = compare_exhaustive_csv(reference_data, comparison_data, args.show_pessimistic)

    #Histograms
    print "Delay Histogram for ", args.reference_csv
    print_delay_histogram(reference_data)

    if args.comparison_csv:
        print
        print "Delay Histogram for ", args.comparison_csv
        print_delay_histogram(comparison_data)

    if args.sta_cpd:
        print
        print "Delay Histogram for STA"
        print_sta_delay_histogram(args.sta_cpd)

    print 
    if safely_pessimistic:
        sys.exit(0)
    else:
        print "ERROR: ESTA results are optimistic or incorrect!"
        sys.exit(1)


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("reference_csv",
                        help="First CSV files to compare")

    parser.add_argument("comparison_csv",
                        nargs="?",
                        default=None,
                        help="Second CSV files to compare")

    parser.add_argument("--sta_cpd",
                        type=float,
                        help="STA critical path delay to place on plot")

    parser.add_argument("--show_pessimistic",
                        default=False,
                        action="store_true",
                        help="Print info about pessimisitic differences")

    args = parser.parse_args()

    return args

def compare_exhaustive_csv(ref_data, cmp_data, show_pessimistic):
    assert ref_data.shape == cmp_data.shape
    #Identify the output column
    ref_col_names = ref_data.columns.values.tolist()
    cmp_col_names = cmp_data.columns.values.tolist()
    ref_output_col_name = None
    ref_input_col_names = None
    cmp_output_col_name = None
    cmp_input_col_names = None
    for i in xrange(len(ref_col_names)):
        ref_col_name = ref_col_names[i]
        cmp_col_name = cmp_col_names[i]
        if ref_col_name == 'delay':
            assert cmp_col_name == ref_col_name
            ref_output_col_name = ref_col_names[i-1]
            ref_input_col_names = ref_col_names[:i-1]
            cmp_output_col_name = cmp_col_names[i-1]
            cmp_input_col_names = cmp_col_names[:i-1]

    #Verify that the inputs are in the same order
    reduced_cmp_col_names = map(lambda x: x.split(":")[0], cmp_input_col_names) #Name is part before ':'
    assert ref_input_col_names == reduced_cmp_col_names, "CSV input columns not in the same order"

    print "Sorting cases"
    #Sort the two data frames so the input transitions are in the same order
    ref_data = ref_data.sort_values(ref_input_col_names)
    cmp_data = cmp_data.sort_values(cmp_input_col_names)

    #Now re-index them so we can walk through them in order by row index
    ref_data.index = range(0,len(ref_data))
    cmp_data.index = range(0,len(cmp_data))

    print "Comparing Input Values"
    if not np.array_equal(ref_data.loc[:,ref_input_col_names].values, cmp_data.loc[:,cmp_input_col_names].values):
        assert False, "Mismtached input transitions"

    print "Comparing Output Transitions"
    #We check that the output transitions agree on the final stable logic value
    # Note that we do not check for precise correspondance (e.g. L <-> L) since
    # the ESTA tool may (pessimistically) report a F (with some delay) where the
    # simulator might produce a L (with zero delay). So long as they agree on the
    # final value everything is still correct, so that is what we check here
    #
    #We do the comparisons in a vectorized manner for speed

    #Build up the boolean selectors for the different cases
    ref_final_high_select = (ref_data[ref_output_col_name] == "R") | (ref_data[ref_output_col_name] == "H")
    ref_final_low_select = (ref_data[ref_output_col_name] == "F") | (ref_data[ref_output_col_name] == "L")
    ref_na_select = (ref_data[ref_output_col_name] == "-")

    cmp_final_high_select = (cmp_data[cmp_output_col_name] == "R") | (cmp_data[cmp_output_col_name] == "H")
    cmp_final_low_select = (cmp_data[cmp_output_col_name] == "F") | (cmp_data[cmp_output_col_name] == "L")
    cmp_na_select = (cmp_data[cmp_output_col_name] == "-")

    #Generate selectors for differences and report errors if any differences are found
    high_mismtach_select = (ref_final_high_select != cmp_final_high_select)
    if high_mismtach_select.any():
        #Not all final 'high' values match
        debug_output_trans_diff(ref_data[high_mismtach_select], cmp_data[high_mismtach_select], ref_input_col_names, ref_output_col_name, cmp_input_col_names, cmp_output_col_name)
        sys.exit(1)

    low_mismtach_select = (ref_final_low_select != cmp_final_low_select)
    if low_mismtach_select.any():
        #Not all final 'low' values match
        debug_output_trans_diff(ref_data[low_mismtach_select], cmp_data[low_mismtach_select], ref_input_col_names, ref_output_col_name, cmp_input_col_names, cmp_output_col_name)
        sys.exit(1)

    na_mismtach_select = (ref_na_select != cmp_na_select)
    if na_mismtach_select.any():
        #Not all final '-' values match
        debug_output_trans_diff(ref_data[na_mismtach_select], cmp_data[na_mismtach_select], ref_input_col_names, ref_output_col_name, cmp_input_col_names, cmp_output_col_name)
        sys.exit(1)

    print "Comparing delays"
    delay_difference = cmp_data['delay'] - ref_data['delay']

    #Filter out exact matches
    delay_difference = delay_difference[delay_difference != 0]

    if not show_pessimistic:
        #Filter out safely pessimsitic values, by keeping only
        #optimistic (negative difference) values
        delay_difference = delay_difference[delay_difference < 0]

    #Filter out differences less than 1ps (i.e. simulation resolution)
    # by keeping differences with magnitude >= 1ps
    delay_difference = delay_difference[abs(delay_difference) >= 1]

    optimistic_count = 0
    for idx, val in delay_difference.iteritems():

        input_trans = ref_data.loc[idx,ref_input_col_names]
        output_trans = ref_data.loc[idx,ref_output_col_name]
        print "Delay diff: {diff:+3.2f} for scenario {input_trans} -> {output_trans}".format(diff=val, input_trans=list(input_trans), output_trans=output_trans)

        if val < 0:
            #ESTA is optimistic!
            optimistic_count += 1

        #Give up if things look bad
        if optimistic_count > 1000:
            print "Gave checking up after 1000 optimistic scenarios..."
            return False

    if optimistic_count > 0:
        return False
    else:
        return True

def debug_output_trans_diff(ref_mismatches, cmp_mismatches, ref_input_col_names, ref_output_col_name, cmp_input_col_names, cmp_output_col_name, mismatch_limit=100):
    rows_to_print = min(ref_mismatches.shape[0], mismatch_limit)
    if ref_mismatches.shape[0] != rows_to_print:
        print "First", rows_to_print, "mismatches shown"
    else:
        print "All", rows_to_print, "mismatches shown"

    for row_idx in ref_mismatches.index:

        #Error!
        print "Mismatched stable output value! ", ref_mismatches.loc[row_idx,ref_input_col_names].values, "->", cmp_mismatches.loc[row_idx,cmp_output_col_name], "(expected", ref_mismatches.loc[row_idx,ref_output_col_name], ")"

def print_delay_histogram(transition_data):
    print "delay prob  count"
    print "----- ----- -----"

    delay_data = transition_data['delay']

    total_cnt = delay_data.shape[0]
    for delay, count in delay_data.value_counts(sort=False).iteritems():
        prob = float(count) / total_cnt
        print "{delay:5} {prob:1.3f} {count:5}".format(delay=delay, prob=prob, count=count)

def print_sta_delay_histogram(sta_cpd):
    print "delay prob  count"
    print "----- ----- -----"
    print "{delay:5} {prob:1.3f} {count:5}".format(delay=sta_cpd, prob=1., count="-")


def load_csv(filename):
    return pd.read_csv(filename)


if __name__ == "__main__":
    main()
