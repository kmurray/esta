#!/usr/bin/env python
import argparse
from collections import OrderedDict
import math

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

from esta_util import load_exhaustive_csv, load_trans_csv, transitions_to_histograms

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("transition_csv",
                        help="Transition csv to convert")

    parser.add_argument("histogram_csv",
                        help="Output histogram csv file")

    parser.add_argument("--keys",
                        default="delay:MAX",
                        nargs="+",
                        help="delay keys to process in transition_csv")

    parser.add_argument("--input_chunk_size",
                        default=100,
                        type=int,
                        help="How many circuit inputs to load at a time (smaller values decrease peak memory usage in exchange for longer run-time")

    return parser.parse_args()

def main():
    args = parse_args()

    hist_dfs = []


    ikey = -1 
    key_groups = []
    for i, key in enumerate(args.keys):

        if i % args.input_chunk_size == 0:
            ikey += 1
            key_groups.append([])

        key_groups[ikey].append(key)


    for key_group in key_groups:

        print "Loading CSV for", key_group
        trans_delay_df = load_trans_csv(args.transition_csv, keys=key_group) 

        print "Converting to Histogram"
        hist_df = transitions_to_histograms(trans_delay_df, keys=key_group)

        hist_dfs.append(hist_df)
        
    print "Merging Dataframes"
    hist_df = pd.concat(hist_dfs, axis=1)
    hist_df.sort_index(inplace=True)
    hist_df.fillna(0., inplace=True)

    print "Writing CSV"
    hist_df.to_csv(args.histogram_csv)

if __name__ == "__main__":
    main()
