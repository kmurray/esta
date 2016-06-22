#!/usr/bin/env python

import math
import subprocess
import argparse
from collections import OrderedDict

import pandas as pd
import numpy as np

import matplotlib.pyplot as plt
#import matplotlib.gridspec as gridspec
from matplotlib.gridspec import GridSpec


from esta_plot import plot_ax, load_histogram_csv
from esta_util import load_trans_csv, transitions_to_histogram

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("simulation_csv",
                        help="Simulation results CSV file")
    parser.add_argument("--chunk_size",
                        default=1e6,
                        help="Number of rows to load in a chunk")
    parser.add_argument("--num_samples",
                        default=10,
                        type=int,
                        help="Number of samples to draw")
    parser.add_argument("--true_max",
                        default=None,
                        help="The true maximum delay")
    parser.add_argument("--base_title",
                        default="",
                        help="Base component of figure title")

    parser.add_argument("--plot_file",
                        default=None,
                        help="Output file")

    parser.add_argument("--search",
                        choices=['mean', 'max'],
                        help="Search for the minimum sample size")

    parser.add_argument("--search_confidence",
                        default=0.95,
                        help="Confidence (mean search mode: interval specification, max search mode: probability have max)")

    parser.add_argument("--search_mean_interval_length",
                        default=0.01,
                        help="Fraction of total delay")

    args = parser.parse_args()

    return args


def main():
    args = parse_args()

    all_data_sets = OrderedDict()

    print "Counting Cases..."
    num_sim_cases, num_inputs, output_name = inspect_sim_file(args.simulation_csv)
    print "Data file contains:", num_sim_cases, "cases"

    print "Loading Data..."
    reader = pd.read_csv(args.simulation_csv, 
                         usecols=['delay'],  #Read in only the delay to save time and memory
                         iterator=True)

    #Read the first element to figure out the header
    df = reader.get_chunk(1)

    num_exhaustive_cases = 4**num_inputs

    print "Inputs: ", num_inputs
    print "Exhaustive cases: {:g}".format(num_exhaustive_cases)
    print "Num Samples: ", args.num_samples

    if args.search is None:
        sample_size = math.floor(num_sim_cases / args.num_samples) #-1 since we read one to get the dimensions
        print "Sample Size: ", sample_size

        sample_frac = sample_size / num_exhaustive_cases
        print "Sample Frac (exhaustive): ", sample_frac
        print "Sample Frac (sim): ", sample_size / num_sim_cases


        sampled_data_sets = generate_samples(args.num_samples, sample_size, reader, args.chunk_size)

        all_data_sets.update(sampled_data_sets)

    elif args.search == 'mean':
        assert False
    elif args.search == 'max':
        assert False
    else:
        assert False



    print "Plotting..."

    nrows = 3
    ncols = 1

    fig = plt.figure(figsize=(8,8))
    fig.suptitle("{base_title} (Sample Size: {sample_size:.2g} ({sample_pct:.2g}%), Num. Samples: {num_samples})".format(base_title=args.base_title, sample_size=sample_size, sample_pct=100*sample_frac, num_samples=args.num_samples), fontsize=14)


    gs = GridSpec(3,1)

    cdf_ax = fig.add_subplot(gs[0,0])

    plot_ax(cdf_ax, all_data_sets, labelx=False, show_legend=False)

    cdf_ax.set_title("Delay CDF")

    cdf_xlim = cdf_ax.get_xlim()

    mean_ax = fig.add_subplot(gs[1,0])
    max_ax = fig.add_subplot(gs[2,0])

    plot_mean(mean_ax, sampled_data_sets)

    plot_max(max_ax, sampled_data_sets, true_max=args.true_max)

    max_ax.set_xlim(cdf_xlim)
    mean_ax.set_xlim(cdf_xlim)

    if args.plot_file:
        plt.savefig(args.plot_file)
    else:
        plt.show()

def generate_samples(num_samples, sample_size, reader, max_chunk_size):
    sampled_data_sets = OrderedDict()

    for i in xrange(num_samples):
        print "Generating Sample", i, "..."

        sample = pd.DataFrame()

        while sample.shape[0] != sample_size:
            #Read a chunk
            chunk_size = min(max_chunk_size, sample_size - sample.shape[0])
            chunk = reader.get_chunk(chunk_size)

            #Concatenate it together
            sample = pd.concat([sample, chunk])

        assert sample.shape[0] == sample_size

        #Convert data sample to histogram
        sample_hist = transitions_to_histogram(sample)

        sampled_data_sets["S" + str(i)] = sample_hist

        #Reset the sample
        sample = pd.DataFrame()

    return sampled_data_sets

def plot_mean(ax, data_sets):
    means = []

    for label, data in data_sets.iteritems():
        means.append( (data['delay'] * data['probability']).sum() )

    df = pd.DataFrame({'mean_delay': means}).sort_values(by='mean_delay')

    df.hist(ax=ax, bins=15)

    ax.set_ylabel("Sample Count")

    ax.set_title("Mean Delay")

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


    ax.stem(max_stats['delay'], max_stats['probability'], label="Sample Max Delays (pdf)", basefmt="")

    if true_max:
        ax.axvline(float(true_max), color="red", label="True Max Delay")

    ax.legend(loc='best')

    ax.set_ylim(ymin=0., ymax=1.)

    ax.set_ylabel("Sample Probability")

    ax.set_xlabel("Delay")
    ax.grid()

    ax.set_title("Max Delay")

def inspect_sim_file(filename):
    extract_cmd = ""
    if filename.endswith(".gz"):
        extract_cmd = "pigz -dc {} 2>/dev/null |".format(filename)

    else:
        extract_cmd = "cat {} | ".format(filename)


    #Determine the number of inputs in this benchmark
    header_cmd = extract_cmd + " head -1"

    result = subprocess.check_output(header_cmd, shell=True)

    cols = result.strip('\n').split(',')

    num_inputs = None
    for i, col_name in enumerate(cols):
        if col_name == 'delay':
            num_inputs = i - 1 #-1 since column before delay is the output

    output_name = cols[num_inputs]

    #Determine number of cases in file
    line_count_cmd = extract_cmd + " wc -l"

    result = subprocess.check_output(line_count_cmd, shell=True)

    result = result.split('\n')[0]

    num_sim_cases = int(result) - 1 #-1 for header

    return num_sim_cases, num_inputs, output_name

if __name__ == "__main__":
    main()
