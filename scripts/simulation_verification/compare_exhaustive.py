#!/usr/bin/env python
import argparse
import pandas as pd
import numpy as np
from itertools import product
from bisect import bisect_left
from collections import OrderedDict
import sys
import pudb
import csv



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

    comparison_data = None
    if args.comparison_csv:
        print "Loading ", args.comparison_csv, "..."
        comparison_data = load_csv(args.comparison_csv)

        print "Comparing ..."
        compare_exhaustive_csv(reference_data, comparison_data, args.show_pessimistic)

    #Histograms
    print "Delay Histogram for ", args.reference_csv
    print_delay_histogram(reference_data)

    if args.comparison_csv:
        print
        print "Delay Histogram for ", args.comparison_csv
        print_delay_histogram(comparison_data)

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("reference_csv",
                        help="First CSV files to compare")

    parser.add_argument("comparison_csv",
                        nargs="?",
                        default=None,
                        help="Second CSV files to compare")

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
            ref_input_col_names = ref_col_names[:i-2]
            cmp_output_col_name = cmp_col_names[i-1]
            cmp_input_col_names = cmp_col_names[:i-2]

    if not np.array_equal(ref_data.loc[:,ref_input_col_names].values, cmp_data.loc[:,cmp_input_col_names].values):
        assert False, "Mismtached input transitions"
        
    if not np.array_equal(ref_data.loc[:,ref_output_col_name].values, cmp_data.loc[:,cmp_output_col_name].values):
        assert False, "Mismtached outputput transitions"

    print "Comparing delays"
    delay_difference = cmp_data['delay'] - ref_data['delay']

    #Filter out exact matches
    delay_difference = delay_difference[delay_difference != 0]

    if not show_pessimistic:
        #Filter out safely pessimsitic values
        delay_difference = delay_difference[delay_difference < 0]

    for idx, val in delay_difference.iteritems():

        input_trans = ref_data.loc[idx,ref_input_col_names]
        output_trans = ref_data.loc[idx,ref_output_col_name]
        print "Delay diff: {diff:+3.2f} for scenario {input_trans} -> {output_trans}".format(diff=val, input_trans=list(input_trans), output_trans=output_trans)

def print_delay_histogram(transition_data):
    print "delay prob  count"
    print "----- ----- -----"

    delay_data = transition_data['delay']

    total_cnt = delay_data.shape[0]
    for delay, count in delay_data.value_counts(sort=False).iteritems():
        prob = float(count) / total_cnt
        print "{delay:5} {prob:1.3f} {count:5}".format(delay=delay, prob=prob, count=count)

def load_csv(filename):
    return pd.read_csv(filename)

def load_csv_old(filename):
    transition_data = {}
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        
        fieldnames = reader.fieldnames
        
        transition_fields = []
        for field in fieldnames:
            if field == "delay":
                break
            transition_fields.append(field)
        data_fields = fieldnames[len(transition_fields):]

        for row in reader:
            transitions = []
            for field in transition_fields:
                transitions.append(row[field])
            #print input_transitions,

            #for data_field in data_fields:
                #print row[data_field],
            #print

            input_transitions = tuple(transitions[:-1])
            output_transition = transitions[-1]
            transition_data[input_transitions] = TransitionScenario(input_transitions, output_transition, float(row['delay']), float(row['exact_prob']), float(row['measured_prob']))

    return transition_data


if __name__ == "__main__":
    main()
