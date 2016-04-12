#!/usr/bin/env python
import argparse
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
    reference_file_data = load_csv(args.reference_csv)

    comparison_file_data = None
    if args.comparison_csv:
        print "Loading ", args.comparison_csv, "..."
        comparison_file_data = load_csv(args.comparison_csv)

        compare_exhaustive_csv(reference_file_data, comparison_file_data, args.show_pessimistic)

    #Histograms
    print "Delay Histogram for ", args.reference_csv
    print_delay_histogram(reference_file_data)

    if comparison_file_data:
        print
        print "Delay Histogram for ", args.comparison_csv
        print_delay_histogram(comparison_file_data)

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

def compare_exhaustive_csv(reference_file, comparison_file, show_pessimistic):


    print "Comparing"
    for input_transitions, reference_scenario in reference_file_data.iteritems():
        comparison_scenario = comparison_file_data[input_transitions]

        assert reference_scenario.output_transition == comparison_scenario.output_transition

        if reference_scenario.delay not in reference_delays:
            reference_delays[reference_scenario.delay] = 0
        reference_delays[reference_scenario.delay] += 1

        if comparison_scenario.delay not in comparison_delays:
            comparison_delays[comparison_scenario.delay] = 0
        comparison_delays[comparison_scenario.delay] += 1

        if reference_scenario.delay != comparison_scenario.delay:
            delay_difference = comparison_scenario.delay - reference_scenario.delay

            if delay_difference < 0 or show_pessimistic:
                print "Delay diff: {diff:+3.2f} for scenario {input_trans} -> {output_trans}".format(diff=delay_difference, input_trans=input_transitions, output_trans=reference_scenario.output_transition)



def print_delay_histogram(transition_data):
    print "delay prob  count"
    print "----- ----- -----"

    delay_counts = {}
    for input_transitions, scenario_data in transition_data.iteritems():
        if scenario_data.delay not in delay_counts:
            delay_counts[scenario_data.delay] = 1
        else:
            delay_counts[scenario_data.delay] += 1

    
    total_cnt = sum(delay_counts.values())

    for delay, count in delay_counts.iteritems():
        prob = float(count) / total_cnt
        print "{delay:5} {prob:1.3f} {count:5}".format(delay=delay, prob=prob, count=count)

def load_csv(filename):
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
