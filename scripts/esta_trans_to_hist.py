#!/usr/bin/env python
import argparse
from collections import OrderedDict
from itertools import cycle
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

    return parser.parse_args()

def main():
    args = parse_args()

    print "Loading CSV"
    trans_delay_df = load_trans_csv(args.transition_csv, keys=args.keys) 

    print "Converting to Histogram"
    hist_df = transitions_to_histograms(trans_delay_df, keys=args.keys)
    
    print "Writing CSV"
    hist_df.to_csv(args.histogram_csv)

if __name__ == "__main__":
    main()
