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

    if args.plot:
        data_sets = OrderedDict()
        data_sets['Modelsim'] = reference_data

        if args.comparison_csv:
            data_sets['ESTA'] = comparison_data

        plot_histogram(args, data_sets)

    print 
    if safely_pessimistic:
        sys.exit(0)
    else:
        print "ERROR: ESTA results are optimistic!"
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

    parser.add_argument("--plot",
                        default=False,
                        action="store_true",
                        help="Plot the delay histograms")

    parser.add_argument("--plot_file",
                        help="File to print plot to")

    parser.add_argument("--plot_bins",
                        default=50,
                        metavar="NUM_BINS",
                        help="How many histogram bins to use")


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
        assert False, "Mismtached output transitions"

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

        if(val < 0):
            #ESTA is optimistic!
            return False

    return True

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


def plot_histogram(args, transition_data_sets):
    color_cycle = cycle("rbgcmyk")
    alpha = 0.8

    plt.figure()

    
    min_val = float("inf")
    max_val = float("-inf")
    for label, transition_data in transition_data_sets.iteritems():
        min_val = min(min_val, transition_data['delay'].min())
        max_val = max(max_val, transition_data['delay'].max())

    if args.sta_cpd:
        min_val = min(min_val, args.sta_cpd)
        max_val = max(max_val, args.sta_cpd)

    histogram_range=(min_val, max_val)

    if args.sta_cpd:
        hist, bins = np.histogram([args.sta_cpd], range=histogram_range, bins=args.plot_bins)

        #Normalize to 1
        hist = hist.astype(np.float32) / hist.sum()

        plt.bar(bins[:-1], hist, width=bins[1] - bins[0], label="STA", color=color_cycle.next(), alpha=alpha)

    for label, transition_data in transition_data_sets.iteritems():
        delay_data = transition_data['delay']

        hist, bins = np.histogram(delay_data.values, range=histogram_range, bins=args.plot_bins)

        #Normalize to 1
        hist = hist.astype(np.float32) / hist.sum()

        plt.bar(bins[:-1], hist, width=bins[1] - bins[0], label=label, color=color_cycle.next(), alpha=alpha)


    plt.ylim(ymax=1.)
    plt.grid()
    plt.ylabel('Probability')
    plt.xlabel('Delay')
    plt.legend(loc='best')

    if args.plot_file:
        plt.savefig(args.plot_file)
    else:
        #Interactive
        plt.show()

if __name__ == "__main__":
    main()
