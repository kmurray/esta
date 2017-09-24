#!/usr/bin/env python
import argparse
from collections import OrderedDict
import math
import re
import os
import sys

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

from esta_util import load_exhaustive_csv, load_trans_csv, transitions_to_histograms

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("histogram_csvs",
                        nargs="+",
                        help="Per-input histogram files")

    return parser.parse_args()

def main():
    args = parse_args()

    name_regex = re.compile(r"esta\.hist\.(?P<output_name>\S+)\.n\d+\.csv")

    hist_dfs = []
    for csv_file in args.histogram_csvs:
        match = name_regex.match(os.path.basename(csv_file))

        assert match, "Could not determine output name from csv file"

        output_name = match.groupdict()['output_name']

        output_df = pd.read_csv(csv_file)
        output_df.set_index("delay", inplace=True)
        output_df.rename(columns={'probability': "prob:{}".format(output_name)}, inplace=True)
        
        hist_dfs.append(output_df)


    hist_df = pd.concat(hist_dfs, axis=1)
    hist_df.sort_index(inplace=True)
    hist_df.fillna(0., inplace=True)

    hist_df.to_csv(sys.stdout)


if __name__ == "__main__":
    main()
