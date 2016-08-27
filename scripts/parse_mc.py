#!/usr/bin/env python
import argparse
import re
import sys
import csv
from collections import OrderedDict

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("mc_log_files",
                        nargs="+",
                        help="Log files to parse")

    return parser.parse_args()

def main():
    args = parse_args()

    regexes = OrderedDict()
    regexes['inputs'] = re.compile(r"Inputs:\s+(\S+)")
    regexes['exhaustive_cases'] = re.compile(r"Exhaustive cases:\s+(\S+)")
    regexes['total_mc_samples'] = re.compile(r"Data file contains:\s+(\S+)\s+cases")
    regexes['specified_max'] = re.compile(r"Specified Max delay:\s+(\S+)")
    regexes['total_mc_max'] = re.compile(r"MC Max delay:\s+(\S+)")
    regexes['total_mc_p_est'] = re.compile(r"P_est:\s+(\S+)")
    regexes['converged_mc_sample_size'] = re.compile(r"Final Sample Size:\s+(\S+)")
    regexes['converged_mc_runtime_sec'] = re.compile(r"MC Runtime \(sec\):\s+(\S+)")
    regexes['not_converged'] = re.compile(r"(Not Converged)")
    regexes['no_max'] = re.compile(r"(Max delay not found in simulation)")

    csv_writer = csv.DictWriter(sys.stdout, fieldnames=['benchmark'] + regexes.keys())
    csv_writer.writeheader()

    name_regex = re.compile(r".*/(\S+).blif")
    for filename in args.mc_log_files:
        results = parse_log(filename, regexes)

        match = name_regex.match(filename)
        assert match
        results['benchmark'] = match.groups()[0]

        csv_writer.writerow(results) 



def parse_log(filename, regexes):
    results = {}
    with open(filename) as f:
        for line in f:
            for key, regex in regexes.iteritems():

                match = regex.match(line)
                if match and key not in results:
                    groups = match.groups()
                    assert len(groups) == 1
                    results[key] = groups[0]
            
    return results

if __name__ == "__main__":
    main()
