#!/usr/bin/env python

import math
import subprocess
import argparse
from collections import OrderedDict

import pandas as pd
import numpy as np
import scipy as sp

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
                        default=0.99,
                        type=float,
                        help="Confidence (mean search mode: interval specification, max search mode: probability have max)")

    parser.add_argument("--search_mean_interval_length",
                        default=5,
                        type=float,
                        help="Interval length to search for")

    parser.add_argument("--search_mean_interval_tol",
                        default=0.01,
                        type=float,
                        help="Error Tolerance in search for mean interval")

    args = parser.parse_args()

    return args


def main():
    args = parse_args()

    all_data_sets = OrderedDict()

    print "Counting Cases..."
    num_sim_cases, num_inputs, output_name = inspect_sim_file(args.simulation_csv)
    print "Data file contains:", num_sim_cases, "cases"

    num_exhaustive_cases = 4**num_inputs

    reader = pd.read_csv(args.simulation_csv, 
                         usecols=['delay'],  #Read in only the delay to save time and memory
                         iterator=True)

    print "Inputs: ", num_inputs
    print "Exhaustive cases: {:g}".format(num_exhaustive_cases)

    if args.search is None:
        print "Num Samples: ", args.num_samples

        sample_size = math.floor(num_sim_cases / args.num_samples)
        print "Sample Size: ", sample_size

        print "Sample Frac (sim): ", sample_size / num_sim_cases


    elif args.search == 'mean':
        sample_size, confidence_interval = search_mean(args.simulation_csv, num_sim_cases, args.search_confidence, args.search_mean_interval_length, args.search_mean_interval_tol, args.chunk_size)

        args.num_samples = int(math.floor(num_sim_cases / float(sample_size)))


    elif args.search == 'max':
        assert False
    else:
        assert False

    sample_frac = sample_size / float(num_exhaustive_cases)
    print "Sample Frac (exhaustive): ", sample_frac

    sampled_data_sets = generate_samples(args.num_samples, sample_size, reader, args.chunk_size)
    all_data_sets.update(sampled_data_sets)

    if args.search == 'mean':
        num_samples_within_confidence = 0
        for k, df in all_data_sets.iteritems():
            sample_mean = (df['delay']*df['probability']).sum() 
            if sample_mean < confidence_interval[0] or sample_mean > confidence_interval[1]:
                print "Warning: sample {} has mean {} outside of confidence interval {}".format(k, sample_mean, confidence_interval)
            else:
                num_samples_within_confidence += 1

        print "Fraction of samples within confidence interval: {} (target confidence level {})".format(float(num_samples_within_confidence) / args.num_samples, args.search_confidence)


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

def search_mean(filename, num_sim_cases, search_confidence, search_mean_interval_len, search_mean_interval_tol, chunk_size):
    """
    Searches various samples sizes to find the smallest size which results in convergence,
    where convergence is defined as having the length of the mean's confidence interval less 
    a target for a given confidence level.

    A binary search on the sample size is used

    Arguments
    =========
        reader: The CSV reader to get data from
        num_sim_cases: Number of simulation cases in the input file
        search_confidence: Confidence level for confidence interval
        search_mean_interval_len: Maximum allowed length for mean confidence interval
    """

    reader = pd.read_csv(filename, 
                         usecols=['delay'],  #Read in only the delay to save time and memory
                         iterator=True)
    #Initial Guess
    sample_size = num_sim_cases

    #Sample
    sample = generate_sample(sample_size, reader, chunk_size)

    #Calculate the interval
    mean, confidence_interval = mean_confidence_interval(sample, search_confidence)

    interval_len = confidence_interval[1] - confidence_interval[0]

    print "Sample Size: {size}, Interval Length Lower Bound: {lb}".format(size=sample_size, 
                                                                          lb=interval_len)

    scale = 2
    meta_scale = 1

    if interval_len > search_mean_interval_len:
        raise ValueError("Insufficient samples for target interval length {len} at confidence {conf}".format(len=search_mean_interval_len, conf=search_confidence))

    prev_shrink = True

    #Shrink the interval length with binary search until it is close to target
    i = 0
    while abs(interval_len - search_mean_interval_len) > search_mean_interval_tol and sample_size > 2 and i < 10000:
        reader = pd.read_csv(filename, 
                             usecols=['delay'],  #Read in only the delay to save time and memory
                             iterator=True)

        #Increase the sample size
        if interval_len < search_mean_interval_len:
            curr_shrink = True
            sample_size = int(sample_size / scale)
            # print "Shrink Sample Size to: ", sample_size
        else:
            assert interval_len > search_mean_interval_len

            curr_shrink = False
            sample_size = int(sample_size * scale)
            # print "Increase Sample Size to: ", sample_size


        assert sample_size <= num_sim_cases

        #Sample
        sample = generate_sample(sample_size, reader, chunk_size)

        #Calculate the interval
        mean, confidence_interval = mean_confidence_interval(sample, search_confidence)

        interval_len = confidence_interval[1] - confidence_interval[0]

        print "Sample Size: {size}, Interval Length: {len}".format(size=sample_size, 
                                                                 len=interval_len)
        if prev_shrink != curr_shrink:
            meta_scale *= 0.75
            scale = 1 + meta_scale
            # print "Next Scale: {}".format(scale)

        i += 1

    print "Sample Size: {size}, Interval: {inter}, Len: {len}".format(size=sample_size,
                                                                      inter=confidence_interval,
                                                                      len=interval_len)
    return sample_size, confidence_interval

def mean_confidence_interval(sample, confidence):
    sample_mean = sample['delay'].mean()
    std_error_mean = sp.stats.sem(sample['delay'])
    dof = len(sample) - 1
    return sample_mean, sp.stats.t.interval(confidence, dof, loc=sample_mean, scale=std_error_mean)


def generate_samples(num_samples, sample_size, reader, max_chunk_size):
    sampled_data_sets = OrderedDict()

    for i in xrange(num_samples):
        print "Generating Sample", i, "..."

        sample = generate_sample(sample_size, reader, max_chunk_size)

        #Convert data sample to histogram
        sample_hist = transitions_to_histogram(sample)

        sampled_data_sets["S" + str(i)] = sample_hist

        #Reset the sample
        sample = pd.DataFrame()

    return sampled_data_sets

def generate_sample(sample_size, reader, max_chunk_size, repeat=False):
    sample = pd.DataFrame()

    while sample.shape[0] != sample_size:
        #Read a chunk
        chunk_size = min(max_chunk_size, sample_size - sample.shape[0])

        chunk = reader.get_chunk(chunk_size)


        #Concatenate it together
        sample = pd.concat([sample, chunk])

    assert sample.shape[0] == sample_size

    return sample


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
