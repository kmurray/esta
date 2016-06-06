#!/usr/bin/env python

import argparse
from collections import OrderedDict

import pandas as pd
import numpy as np

import matplotlib.pyplot as plt
#import matplotlib.gridspec as gridspec
from matplotlib.gridspec import GridSpec


from esta_plot import plot_ax, load_histogram_csv, transitions_to_histogram

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("simulation_csv",
                        help="Simulation results CSV file")
    parser.add_argument("esta_histogram_csv",
                        nargs="?",
                        help="ESTA histogram CSV file")
    parser.add_argument("--sample_frac",
                        default=1e-3,
                        type=float,
                        help="Sample size as fraction of sample space")
    parser.add_argument("--num_samples",
                        default=10,
                        type=int,
                        help="Number of samples to draw")

    args = parser.parse_args()

    return args


def main():
    args = parse_args()


    print "Loading Data..."
    sim_data = pd.read_csv(args.simulation_csv)

    complete_data_sets = OrderedDict()
    sampled_data_sets = OrderedDict()

    print "Exhaustive Histogram..."
    complete_data_sets['Exhaustive'] = transitions_to_histogram(sim_data)

    if args.esta_histogram_csv:
        print "ESTA Histogram..."
        complete_data_sets['ESTA'] = load_histogram_csv(args.esta_histogram_csv)

        #assert np.isclose(max(complete_data_sets['Exhaustive']['delay']), max(complete_data_sets['ESTA']['delay']))

    for i in xrange(args.num_samples):
        print "Generating Sample", i, "..."
        sample = sim_data.sample(frac=args.sample_frac)

        sample_hist = transitions_to_histogram(sample)

        sampled_data_sets["S" + str(i)] = sample_hist

    all_data_sets = complete_data_sets.copy()
    all_data_sets.update(sampled_data_sets)

    print "Plotting..."

    nrows = 3
    ncols = 1

    fig = plt.figure(figsize=(8,8))


    gs = GridSpec(3,1)

    cdf_ax = fig.add_subplot(gs[0,0])

    plot_ax(cdf_ax, all_data_sets)

    cdf_xlim = cdf_ax.get_xlim()

    mean_ax = fig.add_subplot(gs[1,0])
    max_ax = fig.add_subplot(gs[2,0])

    plot_mean(mean_ax, sampled_data_sets)

    plot_max(max_ax, sampled_data_sets, true_max=max(complete_data_sets['Exhaustive']['delay']))

    max_ax.set_xlim(cdf_xlim)
    mean_ax.set_xlim(cdf_xlim)

    plt.show()

def plot_mean(ax, data_sets):
    means = []

    for label, data in data_sets.iteritems():
        means.append( (data['delay'] * data['probability']).sum() )

    df = pd.DataFrame({'mean_delay': means}).sort_values(by='mean_delay')

    df.hist(ax=ax, bins=15)

    ax.set_ylabel("Sample Count")

    #ax.set_title(None)

    #value_counts = df['mean_delay'].value_counts()
    #ax.stem(value_counts.index, value_counts.values)

    #print df.sort_values(by='mean_delay')

def plot_max(ax, sampled_data_sets, true_max):
    maxes = []

    for label, data in sampled_data_sets.iteritems():
        maxes.append( data['delay'].values.max() )

    df = pd.DataFrame({'max_delay': maxes}).sort_values(by='max_delay')

    value_counts = df['max_delay'].value_counts()

    max_stats = pd.DataFrame({'delay': value_counts.index, 'probability': value_counts.values / float(value_counts.sum())})

    max_stats = max_stats.sort_values(by='delay')

    max_stats['cumulative_probability'] = max_stats['probability'].cumsum()


    ax.stem(max_stats['delay'], max_stats['probability'], label="Sample's Max Delay (pdf)", basefmt="")

    ax.axvline(true_max, color="red", label="True Max Delay")
    #ax.plot(max_stats['delay'], max_stats['cumulative_probability'], color="green", label="Sample's Max Delay (cdf)")

    ax.legend(loc='best')

    ax.set_ylim(ymin=0., ymax=1.)

    ax.set_ylabel("Sample Probability")

    

if __name__ == "__main__":
    main()
