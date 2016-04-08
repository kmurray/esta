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

    compare_exhaustive_csv(args.csv_file1, args.csv_file2)

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("csv_file1",
                        help="First CSV files to compare")

    parser.add_argument("csv_file2",
                        help="Second CSV files to compare")

    args = parser.parse_args()

    return args

def compare_exhaustive_csv(file1, file2):

    print "Loading ", file1, "..."
    file1_data = load_csv(file1)
    print "Loading ", file2, "..."
    file2_data = load_csv(file2)

    print "Comparing"
    for input_transitions, first_scenario in file1_data.iteritems():
        second_scenario = file2_data[input_transitions]

        assert first_scenario.output_transition == second_scenario.output_transition

        if first_scenario.delay != second_scenario.delay:
            delay_difference = first_scenario.delay - second_scenario.delay

            print "Delay diff: {diff:+3.2f} for scenario {input_trans} -> {output_trans}".format(diff=delay_difference, input_trans=input_transitions, output_trans=first_scenario.output_transition)


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
