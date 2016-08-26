#!/usr/bin/env python
import argparse
from collections import OrderedDict
from itertools import cycle
import math

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

from esta_util import load_exhaustive_csv

from pylab import rcParams

rcParams['figure.figsize'] = 8, 6

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("histograms",
                        nargs="+",
                        default=[],
                        help="Histogram csv files")

    return parser.parse_args()

def main():
    args = parse_args()

    benchmarks = ['alu4', 'apex2', 'apex4', 'bigkey', 'clma', 'des', 'diffeq', 'dsip', 'elliptic', 'ex1010', 'ex5p', 'frisc', 'misex3', 'pdc', 's298', 's38417', 's38584.1', 'seq', 'spla', 'tseng']

    for csv_filename in args.histograms:
        max_true_delay, max_delay = identify_true_cpd(csv_filename)

        if max_true_delay == max_delay:
            print "{} NoFP {}".format(csv_filename, max_true_delay)
        else:
            print "{} YesFP {}".format(csv_filename, max_true_delay)

def identify_true_cpd(csv_filename):
    df = pd.read_csv(csv_filename)

    max_delay = df['delay:MAX'].max()

    df_non_zero_p = df[df['probability'] != 0.]

    max_true_delay = df_non_zero_p['delay:MAX'].max()

    assert max_delay >= max_true_delay

    return max_true_delay, max_delay

if __name__ == "__main__":
    main()
