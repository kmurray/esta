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

    parser.add_argument("--plot",
                        choices=["hist", "cdf", "stem", "stem_cdf"],
                        default="cdf",
                        help="Type of plot. Default: %(default)s")

    parser.add_argument("--plot_title",
                        default=None,
                        help="Plot title")

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
        data_sets['STA'] = pd.DataFrame({'delay:MAX': [0., args.sta_cpd], 'probability': [0., 1.]})

    for i in xrange(len(args.exhaustive_csvs)):
        label = args.exhaustive_csvs[i] #Default the file name
        if i < len(args.exhaustive_csv_labels):
            #Use specified label
            label = args.exhaustive_csv_labels[i]

        data_sets[label] = load_exhaustive_csv(args.exhaustive_csvs[i])

    for i in xrange(len(args.histogram_csvs)):
        label = args.histogram_csvs[i] #Default the file name
        if i < len(args.histogram_csv_labels):
            #Use specified label
            label = args.histogram_csv_labels[i]

        data_sets[label] = load_histogram_csv(args.histogram_csvs[i])

    sns.set_palette(sns.color_palette("husl", len(data_sets)))

    #sns.palplot(sns.color_palette())

    fig = plt.figure()

    ax = fig.add_subplot(1, 1, 1)

    plot_ax(ax, data_sets, plot_style=args.plot, plot_title=args.plot_title)

    if args.plot_file:
        plt.savefig(args.plot_file)
    else:
        #Interactive
        plt.show()

def load_histogram_csv(filename):
    print "Loading " + filename + "..."
    return pd.read_csv(filename).sort_values(by="delay:MAX")

def map_to_bins(delay_prob_data, bins, histogram_range):

    heights = [0]*len(bins)

    bin_width = bins[1] - bins[0]

    for i, delay, prob in delay_prob_data.itertuples():
        bin_loc = (delay - histogram_range[0])/bin_width
        bin_index = int(math.floor(bin_loc))
        if bin_index == len(bins) - 1:
            bin_index -= 1
            print "Delay: {delay}, Bin: {i} [{min}, {max}]".format(delay=delay, i=bin_index, min=bins[bin_index], max=bins[bin_index+1])
        else:
            print "Delay: {delay}, Bin: {i} [{min}, {max})".format(delay=delay, i=bin_index, min=bins[bin_index], max=bins[bin_index+1])
        heights[bin_index] += prob

    assert np.isclose(sum(heights), 1.)

    return heights

def plot_ax(ax, data_sets, plot_style="cdf",  plot_title=None, labelx=True, labely=True, show_legend=True):
    color_cycle = cycle("rbgcmyk")
    alpha = 0.6

    for label, delay_prob_data in data_sets.iteritems():

        color = color_cycle.next()

        max_delay = delay_prob_data['delay:MAX'].values.max()


        if "stem" in plot_style:

            #ax.stem(delay_prob_data['delay:MAX'], delay_prob_data['probability'], label=label + " (pdf)", linefmt=color+'-', markerfmt=color+"o", basefmt=color+'-', alpha=alpha)
            ax.stem(delay_prob_data['delay:MAX'], delay_prob_data['probability'], label=label + " (pdf)")

        if "cdf" in plot_style:
            #Calculate the CDF
            delay_prob_data['cumulative_probability'] = delay_prob_data['probability'].cumsum()

            #ax.step(x=delay_prob_data['delay:MAX'], y=delay_prob_data['cumulative_probability'], where='post', label=label + " (cdf)", color=color)
            ax.step(x=delay_prob_data['delay:MAX'], y=delay_prob_data['cumulative_probability'], where='post', label=label + " (cdf)")

    ax.set_ylim(ymin=0., ymax=1.)
    #ax.grid()

    if labely:
        ax.set_ylabel('Probability')

    if labelx:
        ax.set_xlabel('Delay')

    if show_legend:
        plt.legend(loc='best', prop={'size': 11})

    if plot_title:
        ax.set_title(plot_title)

if __name__ == "__main__":
    main()
