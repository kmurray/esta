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
        data_sets['STA'] = pd.DataFrame({'delay': [0., args.sta_cpd], 'probability': [0., 1.]})

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


    plot(data_sets, plot_style=args.plot, num_bins=args.plot_bins, plot_file=args.plot_file, plot_title=args.plot_title)

def load_histogram_csv(filename):
    print "Loading " + filename + "..."
    return pd.read_csv(filename).sort_values(by="delay")

def load_exhaustive_csv(filename):
    print "Loading " + filename + "..."
    raw_data = pd.read_csv(filename)


    return transitions_to_histogram(raw_data)

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


def plot(ax, data_sets, plot_style="cdf", plot_title=None, plot_file=None, num_bins=50, print_data=False, show_legend=True):

    #fig = plt.figure()

    

    for label, delay_prob_data in data_sets.iteritems():
        if print_data:
            print label
            print delay_prob_data
            print

        color = color_cycle.next()

        max_delay = delay_prob_data['delay'].values.max()


        if plot_style == "hist":
            min_val = float("inf")
            max_val = float("-inf")
            for label, delay_prob_data in data_sets.iteritems():
                min_val = min(min_val, delay_prob_data['delay'].min())
                max_val = max(max_val, delay_prob_data['delay'].max())

            histogram_range=(min_val, max_val)

            #Generate the bins
            bin_width = float(histogram_range[1] - histogram_range[0]) / num_bins
            bins = [histogram_range[0]] #Initialize at low end of range
            for i in xrange(num_bins-1):
                bins.append(bins[-1] + bin_width)
            bins.append(histogram_range[1]) #End at high end of range, we do this explicitly to avoid FP round-off issues

            if print_data:
                print histogram_range
                print bins

            assert bins[-1] >= histogram_range[1]

            vtext_margin = 4*0.008

            vline_text_y = 1. + vtext_margin

            #Veritical line marking the maximum delay point
            vline_text_y = draw_vline(max_delay, label, color, vline_text_y, vtext_margin)
            heights = map_to_bins(delay_prob_data, bins, histogram_range)

            plt.bar(bins, heights, width=bin_width, label=label, color=color, alpha=alpha)

        if "stem" in plot_style:

            plt.stem(delay_prob_data['delay'], delay_prob_data['probability'], label=label + " (pdf)", linefmt=color+'-', markerfmt=color+"o", basefmt=color+'-', alpha=alpha)

        if "cdf" in plot_style:
            #Calculate the CDF
            delay_prob_data['cumulative_probability'] = delay_prob_data['probability'].cumsum()

            plt.step(x=delay_prob_data['delay'], y=delay_prob_data['cumulative_probability'], where='post', label=label + " (cdf)", color=color)

    plt.ylim(ymin=0., ymax=1.)
    plt.grid()

    plt.ylabel('Probability')

    plt.xlabel('Delay')
    if show_legend:
        plt.legend(loc='best', prop={'size': 11})

    if plot_title:
        plt.title(plot_title, loc="left")

    if plot_file:
        plt.savefig(plot_file)
    else:
        #Interactive
        plt.show()

def plot_ax(ax, data_sets, plot_style="cdf"):
    color_cycle = cycle("rbgcmyk")
    alpha = 0.6

    for label, delay_prob_data in data_sets.iteritems():

        color = color_cycle.next()

        max_delay = delay_prob_data['delay'].values.max()


        if "stem" in plot_style:

            ax.stem(delay_prob_data['delay'], delay_prob_data['probability'], label=label + " (pdf)", linefmt=color+'-', markerfmt=color+"o", basefmt=color+'-', alpha=alpha)

        if "cdf" in plot_style:
            #Calculate the CDF
            delay_prob_data['cumulative_probability'] = delay_prob_data['probability'].cumsum()

            ax.step(x=delay_prob_data['delay'], y=delay_prob_data['cumulative_probability'], where='post', label=label + " (cdf)", color=color)

    ax.set_ylim(ymin=0., ymax=1.)
    ax.grid()

    ax.set_ylabel('Probability')

    ax.set_xlabel('Delay')

def draw_vline(xval, label, color, vline_text_y, vtext_margin):
    #Veritical line marking the maximum delay point
    # We draw this first, so it is covered by the histogram
    plt.axvline(xval, color=color, linestyle='dashed')

    #Label it manually
    plt.text(xval, vline_text_y, label, size=9, rotation='horizontal', color=color, horizontalalignment='center', verticalalignment='top')

    return vline_text_y + vtext_margin

if __name__ == "__main__":
    main()
