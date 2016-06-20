#!/usr/bin/env python
import argparse
from collections import OrderedDict
from itertools import cycle
import math

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("exhaustive_csv",
                        help="Exhaustive csv to convert")

    parser.add_argument("histogram_csv",
                        help="Output histogram csv file")

    return parser.parse_args()

def main():
    args = parse_args()

    exhaustive_csv_to_histogram_csv(args.exhaustive_csv, args.histogram_csv)

def exhaustive_csv_to_histogram_csv(exhaustive_csv_filename, histogram_csv_filename):
    histo_df = load_exhaustive_csv(exhaustive_csv_filename)

    histo_df.to_csv(histogram_csv_filename, index=False)

def load_exhaustive_csv(filename):
    print "Loading " + filename + "..."
    raw_data = pd.read_csv(filename)

    return transitions_to_histogram(raw_data)

def transitions_to_histogram(raw_data):
    #Counts of how often all delay values occur
    raw_counts = raw_data['delay'].value_counts(sort=False)
    
    #Normalize by total combinations (i.e. number of rows)
    #to get probability
    normed_counts = raw_counts / raw_data.shape[0]

    df = pd.DataFrame({"delay": normed_counts.index, "probability": normed_counts.values})

    #Is there a zero probability entry?
    if not df[df['delay'] == 0.].shape[0]:
        #If not, add a zero delay @ probability zero if none is recorded
        #this ensures matplotlib draws the CDF correctly
        zero_delay_df = pd.DataFrame({"delay": [0.], "probability": [0.]})
        
        df = df.append(zero_delay_df)

    return df.sort_values(by="delay")

if __name__ == "__main__":
    main()
