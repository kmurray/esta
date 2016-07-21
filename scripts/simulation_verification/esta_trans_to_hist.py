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



if __name__ == "__main__":
    main()
