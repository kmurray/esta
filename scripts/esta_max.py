#!/usr/bin/env python


import argparse
from collections import OrderedDict
from itertools import cycle
import math

import pandas as pd
import numpy as np
import scipy as sp

from pyemd import emd

from esta_util import load_histogram_csv, load_exhaustive_csv, transitions_to_histogram

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("-i", "--input_csvs",
                        nargs="+",
                        required=True,
                        help="Input CSVs")
    parser.add_argument("-o", "--output_max_trans",
                        required=True,
                        help="Output max histogram")

    return parser.parse_args()

def main():
    args = parse_args()

    max_hist = None

    #Read the input exhaustive results
    input_trans_csvs = []
    for input_file in args.input_csvs:
        trans_df = load_histogram_csv(input_file)
        input_trans_csvs.append(trans_df)

    #Calculate the maximum
    max_trans = max_transition(input_trans_csvs)

    max_trans.to_csv(args.output_max_trans, index=False)

def max_transition(trans_results):
    series = [x['delay'] for x in trans_results]

    #build a data frame of the series
    series_df = pd.DataFrame(series)
    max_delay = series_df.max() #Reduce it down to the max
    
    #Generate a new composite data frame for the max
    max_trans = trans_results[0].copy()

    #Rename the output column
    for i, col in enumerate(max_trans.columns):
        if col == 'delay':
            max_trans.rename(columns={max_trans.columns[i-1]: "max_output_trans"}, inplace=True)
            break

    #Mark output transitions as invalid (since they are now meaningless)
    max_trans['max_output_trans'] = max_trans['max_output_trans'].map(lambda x: '-')

    max_trans['delay'] = max_delay

    return max_trans

if __name__ == "__main__":
    main()
