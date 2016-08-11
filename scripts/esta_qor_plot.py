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

    parser.add_argument("--series_csvs",
                        nargs="*",
                        default=[],
                        help="Histogram csv files to plot")

    parser.add_argument("--plot_title",
                        default=None,
                        help="Plot title")

    parser.add_argument("--plot_file",
                        help="File to print plot to; interactive if unspecified.")

    return parser.parse_args()

def main():
    args = parse_args()

    series = OrderedDict()
    for csv in args.series_csvs:
        print "Loading {}".format(csv)
        d_str, m_str = csv[:-4].split("_")
        d = int(d_str[1:])
        m = float(m_str[1:])

        df = pd.read_csv(csv)

        if d not in series:
            series[d] = OrderedDict()

        series[d][m] = df



    fig = plt.figure()
    color_cycle = cycle("rbgcmyk")

    first = True

    d_init = None
    m_init = None
    for d, m_dict in series.iteritems():
        if d_init is None:
            d_init = d

        color = color_cycle.next()
        symbol_cycle = cycle("ovs*D")
        for m, df in m_dict.iteritems():
            if m_init is None:
                m_init = m

            x_values = df['esta_time_sec'].values
            y_values = df['esta_norm_qor_emd'].values
            if m == 0.0:
                m = float("inf")
            plt.scatter(x_values, y_values, s=40, color=color, marker=symbol_cycle.next(), label="d={} m={:.0e} ({})".format(d, m, df.shape[0]))
            print df.shape

    mc_x_values = series[d_init][m_init]['mc_time_sec'].values
    mc_y_values = [0 for x in mc_x_values]
    plt.scatter(mc_x_values, mc_y_values, s=40, color=color_cycle.next(), marker=symbol_cycle.next(), label="MC mean".format(d, m))
    first = False


    plt.legend(loc='upper left', ncol=1, prop={'size': 10})
    plt.ylim(ymin=0)
    # plt.xlim(xmin=0)
    plt.xlim(xmin=1)
    plt.xscale('log')

    plt.xlabel("Runtime (sec)")
    plt.ylabel("ESTA EMD / STA EMD (QoR)")

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
