#!/usr/bin/env python
import argparse
import re
import sys
import csv
import os
import pandas as pd
from collections import OrderedDict

from esta_qor import hist_emd

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("log_files",
                        nargs="+",
                        help="Log files to parse")

    parser.add_argument("--mc_results_dir",
                        default="/scratch/kmurray/work/esta/mc_results/extract/",
                        help="Directory of MC results")

    return parser.parse_args()

def main():
    args = parse_args()

    regexes = OrderedDict()
    regexes['benchmark'] = re.compile(r"\s*Parsing file: .*/(\S+)\.blif")
    regexes['inputs'] = re.compile(r"\s*Inputs\s*:\s+(\S+)")
    regexes['sta_cpd'] = re.compile(r"\s*STA CPD:\s+(\S+)")
    regexes['s'] = re.compile(r"\s*Slack Threshold\s*:\s+(\S+)")
    regexes['d_coarse'] = re.compile(r"\s*Delay Bin Size Coarse \(below threshold\)\s*:\s+(\S+)")
    regexes['d_fine'] = re.compile(r"\s*Delay Bin Size Fine \(above threshold\)\s*:\s+(\S+)")
    regexes['m'] = re.compile(r"\s*Max Permutations:\s+(\S+)")
    regexes['sta_sec'] = re.compile(r"\s*### End   STA Analysis after\s+(\S+) sec")
    regexes['esta_traversal_sec'] = re.compile(r"\s*### End   ESTA Analysis after\s+(\S+) sec")
    regexes['esta_max_hist_sec'] = re.compile(r"\s*### End   Max histogram after\s+(\S+) sec")
    regexes['esta_output_hists_sec'] = re.compile(r"\s*### End   Output Results after\s+(\S+) sec")
    regexes['esta_total_sec'] = re.compile(r"\s*### End   ETA Application after\s+(\S+) sec")
    regexes['sta_qor_emd'] = None
    regexes['esta_qor_emd'] = None
    regexes['esta_qor_norm_emd'] = None

    csv_writer = csv.DictWriter(sys.stdout, fieldnames=regexes.keys())
    csv_writer.writeheader()

    results = []
    for filename in args.log_files:
        log_results = parse_log(filename, regexes, args.mc_results_dir)
        results.append(log_results)

    results = sorted(results, key=lambda result: int(result['inputs']))
    for row in results:
        csv_writer.writerow(row) 



def parse_log(filename, regexes, mc_results_dir):
    results = {}
    with open(filename) as f:
        for line in f:
            for key, regex in regexes.iteritems():

                if regex:
                    match = regex.match(line)
                    if match and key not in results:
                        groups = match.groups()
                        assert len(groups) == 1
                        results[key] = groups[0]
            
    run_dir = os.path.dirname(filename)
    esta_histo_filename = os.path.join(run_dir, "esta.max_hist.csv")
    mc_histo_filename = os.path.join(mc_results_dir, results['benchmark'] + ".blif", 'sim_mc.max_hist.full.csv')

    df_mc_histo = pd.read_csv(mc_histo_filename)

    df_sta_histo = None
    if 'sta_cpd' in results:
        df_sta_histo = pd.DataFrame([[0.,0.], 
                                     [float(results['sta_cpd']), 1.]], 
                                     columns=['delay:MAX', 'probability'])


        results['sta_qor_emd'] = hist_emd(df_mc_histo, df_sta_histo)

    df_esta_histo = None
    if os.path.exists(esta_histo_filename):
        df_esta_histo = pd.read_csv(esta_histo_filename)

        results['esta_qor_emd'] = hist_emd(df_mc_histo, df_esta_histo)

    if 'esta_qor_emd' in results and 'sta_qor_emd' in results:
        results['esta_qor_norm_emd'] = results['esta_qor_emd'] / results['sta_qor_emd']

    return results

if __name__ == "__main__":
    main()
