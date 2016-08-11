#!/usr/bin/env python


import argparse
from collections import OrderedDict
from itertools import cycle
import math

import pandas as pd
import numpy as np
import scipy as sp

from pyemd import emd

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("reference_csv",
                        help="Reference Histogram CSV to compare against")

    parser.add_argument("compare_csvs",
                        nargs="+",
                        help="Histogram CSVs to cmpare with reference")


    return parser.parse_args()

def main():
    args = parse_args()

    reference_hist = pd.read_csv(args.reference_csv)

    for compare_csv in args.compare_csvs:
        #TODO: for now just do one comparison
        compare_hist = pd.read_csv(compare_csv)

        #compare_hist['delay'] = compare_hist['delay'].round(0).astype(int)

        #print reference_hist
        #print compare_hist

        #Merge the two columns on the union of delays
        merged_df = pd.merge(reference_hist, compare_hist, how='outer', on=['delay'])

        #Treat missing values as zero
        merged_df = merged_df.fillna(0)

        distance_matrix = np.zeros(shape=(merged_df.shape[0], merged_df.shape[0]))

        for i, ival in enumerate(merged_df['delay'].values):
            for j, jval in enumerate(merged_df['delay'].values):
                distance_matrix[i][j] = abs(ival - jval)
                #print i, j, ival - jval

        #print distance_matrix

        print "EMD {}: ".format(compare_csv), emd(merged_df['probability_x'].values, merged_df['probability_y'].values, distance_matrix)

def hist_emd(reference_hist_df, compare_hist_df):
    #Merge the two columns on the union of delays
    merged_df = pd.merge(reference_hist_df, compare_hist_df, how='outer', on=['delay:MAX'])

    #Treat missing values as zero
    merged_df = merged_df.fillna(0)

    distance_matrix = np.zeros(shape=(merged_df.shape[0], merged_df.shape[0]))

    for i, ival in enumerate(merged_df['delay:MAX'].values):
        for j, jval in enumerate(merged_df['delay:MAX'].values):
            distance_matrix[i][j] = abs(ival - jval)

    return emd(merged_df['probability_x'].values, merged_df['probability_y'].values, distance_matrix)

if __name__ == "__main__":
    main()
