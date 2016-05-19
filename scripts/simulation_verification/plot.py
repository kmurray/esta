#!/usr/bin/env python
import argparse
from collections import OrderedDict
from itertools import cycle
import math

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("--histogram_csvs",
                        nargs="*",
                        default=[],
                        help="Histogram csv files to plot")

    parser.add_argument("--histogram_csv_labels",
                        nargs="*",
                        default=[],
                        help="Label for each histrogram csv file")

    parser.add_argument("--exhaustive_csvs",
                        nargs="*",
                        default=[],
                        help="Exhaustive csv files to plot")

    parser.add_argument("--exhaustive_csv_labels",
                        nargs="*",
                        default=[],
                        help="Label for each histrogram csv file")

    parser.add_argument("--sta_cpd",
                        type=float,
                        help="STA critical path delay to place on plot")

    parser.add_argument("--plot_file",
                        help="File to print plot to; interactive if unspecified.")

    parser.add_argument("--plot_bins",
                        metavar="NUM_BINS",
                        default=50,
                        type=float,
                        help="How many histogram bins to use")

    return parser.parse_args()

def main():
    args = parse_args()

    data_sets = OrderedDict()

    if args.sta_cpd:
        data_sets['STA'] = pd.DataFrame({'delay': [args.sta_cpd], 'probability': [1.]})

    for i in xrange(len(args.histogram_csvs)):
        label = args.histogram_csvs[i] #Default the file name
        if i < len(args.histogram_csv_labels):
            #Use specified label
            label = args.histogram_csv_labels[i]

        data_sets[label] = load_histogram_csv(args.histogram_csvs[i])

    for i in xrange(len(args.exhaustive_csvs)):
        label = args.exhaustive_csvs[i] #Default the file name
        if i < len(args.exhaustive_csv_labels):
            #Use specified label
            label = args.exhaustive_csv_labels[i]

        data_sets[label] = load_exhaustive_csv(args.exhaustive_csvs[i])



    plot_histogram(data_sets, num_bins=args.plot_bins, plot_file=args.plot_file)

def load_histogram_csv(filename):
    return pd.read_csv(filename)

def load_exhaustive_csv(filename):
    raw_data = pd.read_csv(filename)

    #Counts of how often all delay values occur
    raw_counts = raw_data['delay'].value_counts(sort=False)
    
    #Normalize by total combinations (i.e. number of rows)
    #to get probability
    normed_counts = raw_counts / raw_data.shape[0]

    return pd.DataFrame({"delay": normed_counts.index, "probability": normed_counts.values})

def map_to_bins(delay_prob_data, num_bins, histogram_range):

    bin_width = float(histogram_range[1] - histogram_range[0]) / num_bins

    bins = [histogram_range[0]]
    while bins[-1] < histogram_range[1]:
        bins.append(bins[-1] + bin_width)
    
    assert np.isclose(bins[-1], histogram_range[1])

    heights = [0]*len(bins)

    for i, delay, prob in delay_prob_data.itertuples():
        bin_index = int(math.floor(delay/bin_width))
        if bin_index == len(bins) - 1:
            bin_index -= 1
            print "Delay: {delay}, Bin: {i} [{min}, {max}]".format(delay=delay, i=bin_index, min=bins[bin_index], max=bins[bin_index+1])
        else:
            print "Delay: {delay}, Bin: {i} [{min}, {max})".format(delay=delay, i=bin_index, min=bins[bin_index], max=bins[bin_index+1])
        heights[bin_index] += prob


    return (bins, heights)

def plot_histogram(data_sets, num_bins=50, plot_file=None):
    color_cycle = cycle("rbgcmyk")
    alpha = 0.6

    fig = plt.figure()

    
    min_val = float("inf")
    max_val = float("-inf")
    for label, delay_prob_data in data_sets.iteritems():
        min_val = min(min_val, delay_prob_data['delay'].min())
        max_val = max(max_val, delay_prob_data['delay'].max())

    histogram_range=(min_val, max_val)

    vtext_margin = 4*0.008

    vline_text_y = 1. + vtext_margin

    for label, delay_prob_data in data_sets.iteritems():
        print label
        print delay_prob_data
        print

        color = color_cycle.next()

        max_delay = delay_prob_data['delay'].values.max()

        #Veritical line marking the maximum delay point
        draw_vline(max_delay, label, color, vline_text_y)
        vline_text_y += vtext_margin #Advance the position for the next vertical line text marking

        bins, heights = map_to_bins(delay_prob_data, num_bins, histogram_range)

        plt.bar(bins, heights, width=bins[1] - bins[0], label=label, color=color, alpha=alpha)

    plt.ylim(ymin=0., ymax=1.)
    plt.grid()
    plt.ylabel('Probability')
    plt.xlabel('Delay')
    plt.legend(loc='best')

    if plot_file:
        plt.savefig(args.plot_file)
    else:
        #Interactive
        plt.show()

def draw_vline(xval, label, color, vline_text_y):
    #Veritical line marking the maximum delay point
    # We draw this first, so it is covered by the histogram
    plt.axvline(xval, color=color, linestyle='dashed')

    #Label it manually
    plt.text(xval, vline_text_y, label, size=9, rotation='horizontal', color=color, horizontalalignment='center', verticalalignment='top')


if __name__ == "__main__":
    main()
