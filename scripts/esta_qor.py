#!/usr/bin/env python


import argparse
from collections import OrderedDict
from itertools import cycle
import math
import json

import pandas as pd
import numpy as np
import scipy as sp

from pyemd import emd

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("reference_csv",
                        help="Reference Histogram CSV to compare against")

    parser.add_argument("compare_csv",
                        help="Histogram CSV to compare with reference")

    parser.add_argument("sta_json",
                        help="JSON file describing per-output STA max delays")

    parser.add_argument("--mode",
                        choices=['max', 'per_output'],
                        help="How to compare the CSVs")


    return parser.parse_args()

def main():
    args = parse_args()

    sta_delays = load_json(args.sta_json)
    sta_hist = build_sta_hist(sta_delays)

    reference_hist = pd.read_csv(args.reference_csv)
    reference_hist.set_index('delay', inplace=True)

    compare_hist = pd.read_csv(args.compare_csv)
    compare_hist.set_index('delay', inplace=True)

    keys = []
    if args.mode == "max":
        keys.append('prob:MAX')
    else:
        assert args.mode == "per_output"

        for col in reference_hist.columns:
            if col != "delay" and col != "prob:MAX":
                keys.append(col)

    normed_emds = {}
    for key in keys:
        ref_sta_emd = hist_emd(reference_hist, sta_hist, key)
        ref_comp_emd = hist_emd(reference_hist, compare_hist, key)

        norm_emd = ref_comp_emd / ref_sta_emd

        print "-------------------------"
        print "Ref-STA EMD for {}: {}".format(key, ref_sta_emd)
        print "Ref-Comp EMD for {}: {}".format(key, ref_comp_emd)
        print "Ref-Comp Norm. EMD for {}: {}".format(key, norm_emd)

        normed_emds[key] = norm_emd

    print "-------------------------"
    arithmean_norm_emd = np.mean(normed_emds.values())
    print "Arithmean Ref-Comp Norm EMD: {}".format(arithmean_norm_emd)

def hist_emd(reference_hist_df, compare_hist_df, key, distance_matrix=None):
    #Merge the two columns on the union of delays
    merged_df = pd.merge(reference_hist_df, compare_hist_df, how='outer', left_index=True, right_index=True)
    merged_df.fillna(0., inplace=True) #Treat missing values as zero

    ref_merged_key = key + '_x'
    comp_merged_key = key + '_y'

    if distance_matrix == None:
        #Unspecified, calculate
        distance_matrix = calc_distance_matrix(merged_df.index, merged_df.index)

    return emd(merged_df[ref_merged_key].values, merged_df[comp_merged_key].values, distance_matrix)

def calc_distance_matrix(ref_values, comp_values):
    distance_matrix = np.zeros(shape=(len(ref_values), len(comp_values)))

    for i, ival in enumerate(ref_values):
        for j, jval in enumerate(comp_values):
            distance_matrix[i][j] = abs(ival - jval)

    return distance_matrix

def load_json(json_file):
    json_data = None
    with open(json_file) as f:
        json_data = json.load(f)

    sta_delays = {}

    for info_dict in json_data['endpoint_timing']:
        sta_delays[info_dict['node_identifier']] = float(info_dict['T_arr'])

    sta_delays['MAX'] = max(sta_delays.values())

    return sta_delays

def build_sta_hist(sta_delays):
    sta_hist_dfs = []

    for output, delay in sta_delays.iteritems():
        delay_ps = int(round(delay / 1.e-12))
        sta_hist_df = pd.DataFrame({'prob:{}'.format(output): [0., 1.], 
                                    'delay': [0, delay_ps]})
        sta_hist_df.set_index('delay', inplace=True)
        
        sta_hist_dfs.append(sta_hist_df)

    sta_hist_df = pd.concat(sta_hist_dfs, axis=1)
    sta_hist_df.sort_index(inplace=True)
    sta_hist_df.fillna(0., inplace=True)

    return sta_hist_df



if __name__ == "__main__":
    main()
