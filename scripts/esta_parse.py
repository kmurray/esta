#!/usr/bin/env python
import os
import sys
import argparse
import re
import pprint
import json
from collections import OrderedDict

import pandas as pd

from esta_qor import hist_emd
from esta_util import create_sta_hist



def main():
    args = parse_args()

    results = OrderedDict()

    for dir in args.run_dirs:
        if os.path.isdir(dir):
            try:
                benchmark, d, m, metrics = parse_dir(dir, args.mc_dir, args.mc_type)
            except IOError as e:
                print "Warning skipping {} ({})".format(dir, e)
                continue


            if d not in results:
                results[d] = OrderedDict()

            if m not in results[d]:
                results[d][m] = pd.DataFrame(columns=metrics.keys())

            results[d][m] = results[d][m].append(pd.Series(metrics, name=benchmark), verify_integrity=True)

    for d, m_dict in results.iteritems():
        for m, df in m_dict.iteritems():
            if m == 0.0:
                m = float("inf")
            df.to_csv("d{}_m{}.csv".format(d, m), index_label='benchmark')

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    parser.add_argument("run_dirs",
                        nargs="+",
                        help="Directories to parse")

    parser.add_argument("--mc_dir",
                        required=True,
                        help="Monte-Carlo run dir (for QoR comparison)")

    parser.add_argument("--mc_type",
                        default="mean",
                        help="Monte-Carlo run type")

    return parser.parse_args()

def parse_dir(esta_dir, mc_base_dir, mc_type):
    
    benchmark, d_str, m_str = esta_dir.split("_")
    d_val = int(d_str[1:])
    m_val = float(m_str[1:])

    mc_dir = os.path.join(mc_base_dir, benchmark + ".blif")

    print "{} d={} m={}".format(benchmark, d_val, m_val)

    esta_runtime_data = parse_esta_runtime(esta_dir)
    mc_runtime_data = parse_mc_runtime(mc_dir, mc_type)

    sta_qor = calc_sta_qor(esta_dir, mc_dir, mc_type)
    esta_qor = calc_esta_qor(esta_dir, mc_dir, mc_type)

    norm_esta_qor = esta_qor / sta_qor

    print "\tESTA Time:", esta_runtime_data['time_sec']
    print "\tMC   Time:", mc_runtime_data['time_sec']
    print "\tESTA QoR :", esta_qor
    print "\tSTA  QoR :", sta_qor
    print "\tESTA_norm QoR :", norm_esta_qor

    metrics = OrderedDict()
    metrics["esta_time_sec"] = esta_runtime_data['time_sec']
    metrics["esta_mem_bytes"] = esta_runtime_data['memory_bytes']
    metrics["mc_time_sec"] = mc_runtime_data['time_sec']
    metrics["esta_qor_emd"] = esta_qor
    metrics["sta_qor_emd"] = sta_qor
    metrics["esta_norm_qor_emd"] = norm_esta_qor

    return benchmark, d_val, m_val, metrics

def parse_esta_runtime(esta_dir):
    runtime_regex = re.compile(r"^### End   ETA Application after (?P<time_sec>\d+.?\d*) sec")
    memory_regex = re.compile(r"^\s*Maximum resident set size \(kbytes\): (?P<mem_kb>\d+)")

    esta_log_filename = os.path.join(esta_dir, "esta.log")

    run_time = None
    memory = None
    try:
        with open(esta_log_filename) as f:
            for line in f:
                match = runtime_regex.match(line)
                if match:
                    run_time = float(match.group("time_sec"))

                match = memory_regex.match(line)
                if match:
                    memory = int(match.group("mem_kb")) * 1024
    except IOError as e:
        print "Failed to open {} ({})".format(esta_log_filename, e)

    result = OrderedDict()
    result["time_sec"] = run_time
    result["memory_bytes"] = memory

    return result

def parse_mc_runtime(mc_dir, mc_type):
    runtime_regex = re.compile(r"^MC Runtime \(sec\): (?P<time_sec>\d+.?\d*)")

    log_filename = os.path.join(mc_dir, "esta_mc." + mc_type + ".log")

    run_time = None
    memory = None
    try:
        with open(log_filename) as f:
            for line in f:
                match = runtime_regex.match(line)
                if match:
                    run_time = float(match.group("time_sec"))

    except IOError as e:
        print "Failed to open ({})".format(e)

    result = OrderedDict()
    result["time_sec"] = run_time

    return result

def parse_sta_cpd(dir):
    vpr_log_filename = os.path.join(dir, "vpr_stdout.log")

    cpd_regex = re.compile(r"^Final critical path: (?P<cpd_ns>\d+.?\d*) ns")

    vpr_cpd = None
    with open(vpr_log_filename) as f:
        for line in f:
            match = cpd_regex.match(line)
            if match:
                vpr_cpd = float(match.group("cpd_ns"))

    return vpr_cpd

def load_mc_hist(mc_dir, mc_type):
    mc_hist_csv = os.path.join(mc_dir, "sim_mc.max_hist." + mc_type + ".csv")
    mc_hist_df = pd.read_csv(mc_hist_csv)

    return mc_hist_df

def load_esta_hist(esta_dir):
    esta_hist_csv = os.path.join(esta_dir, "esta.max_hist.csv")
    esta_hist_df = pd.read_csv(esta_hist_csv)

    esta_hist_df.rename(columns={'delay': 'delay:MAX'}, inplace=True)

    return esta_hist_df

def calc_sta_qor(esta_dir, mc_dir, mc_type):

    esta_run_sta_cpd = parse_sta_cpd(esta_dir)
    mc_run_sta_cpd = parse_sta_cpd(mc_dir)

    if esta_run_sta_cpd != mc_run_sta_cpd:
        print "WARNING: STA CPD differs: {} != {}".format(esta_run_sta_cpd, mc_run_sta_cpd)

    sta_hist_df = create_sta_hist(esta_run_sta_cpd)

    mc_hist_df = load_mc_hist(mc_dir, mc_type)

    return hist_emd(mc_hist_df, sta_hist_df)

def calc_esta_qor(esta_dir, mc_dir, mc_type):
    mc_hist_df = load_mc_hist(mc_dir, mc_type)

    esta_hist_df = load_esta_hist(esta_dir)

    return hist_emd(mc_hist_df, esta_hist_df)


if __name__ == "__main__":
    main()
