#!/usr/bin/env python

import sys
import math
import subprocess
import argparse
from collections import OrderedDict, deque
from decimal import Decimal

import pandas as pd
import numpy as np
import scipy as sp
import statsmodels.stats.proportion as sms_sp


import matplotlib.pyplot as plt
#import matplotlib.gridspec as gridspec
from matplotlib.gridspec import GridSpec


from esta_plot import plot_ax, load_histogram_csv
from esta_util import load_trans_csv, transitions_to_histogram

class NotConvergedException(Exception):
    pass

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("simulation_csv",
                        help="Simulation results CSV file")

    parser.add_argument("--search",
                        choices=['mean', 'max'],
                        default='max',
                        help="Search for the minimum sample size")

    parser.add_argument("--search_confidence",
                        default=0.99,
                        type=float,
                        help="Confidence (used during interval calculation)")

    parser.add_argument("--search_mean_interval_len",
                        default=5,
                        type=float,
                        help="Acceptable mean delay confidence interval length")

    parser.add_argument("--search_max_p_rel_interval_len",
                        default=0.03,
                        type=float,
                        help="Acceptable max delay relative probability confidence interval length")

    parser.add_argument("--true_max",
                        default=None,
                        help="The true maximum delay")

    parser.add_argument("--base_title",
                        default="",
                        help="Base component of figure title")

    parser.add_argument("--plot_file",
                        default=None,
                        help="Output file, or 'interactive' for an interactive plot")

    parser.add_argument("--num_samples",
                        default=10,
                        type=int,
                        help="Number of samples to draw for plotting")


    args = parser.parse_args()

    return args


def main():
    args = parse_args()

    print " ".join(sys.argv)

    all_data_sets = OrderedDict()

    print "Counting Cases..."
    num_sim_cases, num_inputs, output_name = inspect_sim_file(args.simulation_csv)
    print "Data file contains:", num_sim_cases, "cases"

    num_exhaustive_cases = Decimal(4)**Decimal(num_inputs)

    # reader = pd.read_csv(args.simulation_csv,
                         # usecols=['delay:MAX'])  #Read in only the delay to save time and memory
                         # iterator=True)

    print "Inputs: ", num_inputs
    print "Exhaustive cases: {:g}".format(num_exhaustive_cases)

    print "Loading delay values"
    df = pd.read_csv(args.simulation_csv, usecols=['delay:MAX'])

    num_samples = args.num_samples
    try:
        if args.search is None:
            print "Num Samples: ", num_samples

            sample_size = math.floor(num_sim_cases / num_samples)
            print "Sample Size: ", sample_size

            print "Sample Frac (sim): ", sample_size / num_sim_cases

        elif args.search == 'mean':
            sample_size, confidence_interval = search_mean(df, num_sim_cases, args.search_confidence, args.search_mean_interval_len)

            num_samples = min(num_samples, int(math.floor(num_sim_cases / float(sample_size))))

            print "Num Samples: ", num_samples
        elif args.search == 'max':

            max_delay = args.true_max
            if max_delay is None:
                max_delay = float(max(df['delay:MAX']))
            else:
                max_delay = float(max_delay)

            sample_size = search_max_prob(df, num_sim_cases, args.search_confidence, args.search_max_p_rel_interval_len, num_samples, max_delay)
        else:
            assert False
    except NotConvergedException as e:
        print "Not Converged"
        print e
        sys.exit()

    sample_frac = sample_size / float(num_exhaustive_cases)
    sim_sample_frac = sample_size / float(num_sim_cases)
    print "Final Sample Size: ", sample_size
    print "Final Sample Frac (simulation): ", sample_size / float(num_sim_cases)

    print "MC Runtime (sec):", sim_sample_frac * (48 * 60 * 60)

    if args.plot_file:

        sampled_data_sets = generate_samples(num_samples, sample_size, df)
        all_data_sets.update(sampled_data_sets)

        if args.search == 'mean':
            num_samples_within_confidence = 0
            for k, df in all_data_sets.iteritems():
                sample_mean = (df['delay:MAX']*df['probability']).sum()
                if sample_mean < confidence_interval[0] or sample_mean > confidence_interval[1]:
                    print "Warning: sample {} has mean {} outside of confidence interval {}".format(k, sample_mean, confidence_interval)
                else:
                    num_samples_within_confidence += 1

            print "Fraction of samples within confidence interval: {} (target confidence level {})".format(float(num_samples_within_confidence) / num_samples, args.search_confidence)

        print "Plotting..."

        nrows = 3
        ncols = 1

        fig = plt.figure(figsize=(8,8))
        fig.suptitle("{base_title} (Sample Size: {sample_size:.2g} ({sample_pct:.2g}%), Num. Samples: {num_samples})".format(base_title=args.base_title, sample_size=sample_size, sample_pct=100*sample_frac, num_samples=num_samples), fontsize=14)


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

        if args.plot_file == "interactive":
            plt.show()
        else:
            plt.savefig(args.plot_file)

        print "Saving Sample 0 Histogram ..."
        sample_hist_0 = sampled_data_sets.values()[0]
        sample_hist_0.to_csv("sim_mc.max_hist." + args.search + ".csv", columns=['delay:MAX', 'probability'], index=False)

def search_mean(df, num_sim_cases, search_confidence, search_mean_interval_len):
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
        search_mean_threshold_ps: Maximum allowed delta for mean confidence interval
    """

    #Binary Search for the minimum sample size with sufficient confidence
    sample_size = num_sim_cases
    lower_bound_sample_size = -1
    upper_bound_sample_size = -1
    interval_len = None
    while lower_bound_sample_size == -1 or upper_bound_sample_size == -1 or (upper_bound_sample_size - lower_bound_sample_size > 1):
        print "Sample Size: {} (Size Bounds: [{},{}])".format(sample_size,
                                                              lower_bound_sample_size,
                                                              upper_bound_sample_size)

        sample = df.sample(sample_size)

        mean, confidence_interval = mean_confidence_interval(sample['delay:MAX'], search_confidence)

        interval_len = confidence_interval[1] - confidence_interval[0]

        step = None
        if interval_len < search_mean_interval_len:
            #Below target
            upper_bound_sample_size = sample_size

            #Decrease sample size
            if lower_bound_sample_size == -1:
                step = -(sample_size / 2)
            else:
                step = -(sample_size - lower_bound_sample_size) / 2

        elif interval_len >= search_mean_interval_len:
            #Above target
            lower_bound_sample_size = sample_size

            #Increase sample size
            if upper_bound_sample_size == -1:
                step = (num_sim_cases - sample_size) / 2
            else:
                step = (upper_bound_sample_size - sample_size) / 2

        assert upper_bound_sample_size >= lower_bound_sample_size

        if step == 0:
            break

        sample_size += step

    assert upper_bound_sample_size != -1
    assert lower_bound_sample_size != -1
    assert upper_bound_sample_size - lower_bound_sample_size <= 1

    print "Minimal Sample Size: {}, Interval len: {} (@ {} confidence)".format(upper_bound_sample_size, interval_len, search_confidence)

    return upper_bound_sample_size, confidence_interval

def determine_p_est_by_interval(df, max_delay, search_confidence, search_max_p_rel_interval_len):
    num_sample_cases = df.shape[0]

    #Estimate probability of getting a max delay path from the entire MC sim
    num_max_delay_cases = df['delay:MAX'].value_counts()[max_delay]

    p_est = num_max_delay_cases / float(num_sample_cases)

    print "P_est: {}".format(p_est)

    #Calculate the interval to see if it has converged
    #
    #The 'beta' (Clopper-Pearson) method is a pessimistic interval which gaurentees
    #to cover the alpha-significant interval, but it may be conservative (i.e. it may
    #cover a more significant (smaller alpha)
    alpha = 1. - search_confidence
    ci = sms_sp.proportion_confint(num_max_delay_cases, num_sample_cases, alpha=alpha, method="beta")

    ci_len = ci[1] - ci[0]
    ci_len_ratio = ci_len / p_est

    print "P_est CI: [{:g}, {:g}] @ alpha={} ci_len/P_est={}".format(ci[0], ci[1], alpha, ci_len_ratio)

    if p_est < ci[0] or p_est > ci[1]:
        msg = "Estimate {:g} falls outside confidence interval [{:g}, {:g}]: NOT CONVERGED".format(p_est, ci[0], ci[1])

        raise NotConvergedException(msg)

    if ci_len_ratio > search_max_p_rel_interval_len:
        msg = "Normalized CI delta (ci[1] - ci[0])/p_ext={:g} exceeds target={:g}: NOT CONVERGED".format(ci_len_ratio, search_max_p_rel_interval_len)

        raise NotConvergedException(msg)


    return p_est, ci


def search_max_prob(df, num_sim_cases, search_confidence, search_max_p_rel_interval_len, num_samples, max_delay):
    """
    Determines the minimal sample size for which the Monte-Carlo simulation maximum delay has converged (in the
    sense max delay having correct probability).

    We determine this by conducting a series of bernoulli (success/fail) experiments, where at different sample
    sizes
    """
    print "Specified Max delay: {}".format(max_delay)

    mc_max_delay = max(df['delay:MAX'])
    print "MC Max delay: {}".format(mc_max_delay)

    if max_delay != float(mc_max_delay):
        msg = "Max delay not found in simulation: NOT CONVERGED"
        raise NotConvergedException(msg)

    p_est, p_est_conf_interval = determine_p_est_by_interval(df, max_delay, search_confidence, search_max_p_rel_interval_len)

    #Binary Search for the minimum sample size with sufficient confidence
    sample_size = num_sim_cases
    lower_bound_sample_size = -1
    upper_bound_sample_size = -1
    while lower_bound_sample_size == -1 or upper_bound_sample_size == -1 or (upper_bound_sample_size - lower_bound_sample_size > 1):

        df_sub_sample = df.sample(sample_size)

        converged = True
        try:
            p_sub_est, p_sub_est_ci = determine_p_est_by_interval(df_sub_sample, max_delay, search_confidence, search_max_p_rel_interval_len)
        except NotConvergedException as e:
            converged = False

        print "Sample Size: {} (Size Bounds: [{},{}])".format(sample_size,
                                                              lower_bound_sample_size,
                                                              upper_bound_sample_size)
        step = None
        if not converged:
            #Below target confidence
            lower_bound_sample_size = sample_size

            #Increase sample size
            if upper_bound_sample_size == -1:
                step = (num_sim_cases - sample_size) / 2
            else:
                step = (upper_bound_sample_size - sample_size) / 2

        else:
            #Above target confidence
            upper_bound_sample_size = sample_size

            #Decrease sample size
            if lower_bound_sample_size == -1:
                step = -(sample_size / 2)
            else:
                step = -(sample_size - lower_bound_sample_size) / 2

        if lower_bound_sample_size == num_sim_cases:
            raise NotConvergedException("Failed to converge".format(p_max, lower_bound_sample_size, search_confidence))

        assert upper_bound_sample_size >= lower_bound_sample_size

        if step == 0:
            break

        sample_size += step

    assert upper_bound_sample_size != -1
    assert lower_bound_sample_size != -1
    assert upper_bound_sample_size - lower_bound_sample_size <= 1

    print "Minimal Sample Size: {} (@ {} confidence)".format(upper_bound_sample_size, search_confidence)

    return upper_bound_sample_size

def mean_confidence_interval(sample, confidence):
    sample_mean = sample.mean()
    std_error_mean = sp.stats.sem(sample)
    dof = len(sample) - 1
    return sample_mean, sp.stats.t.interval(confidence, dof, loc=sample_mean, scale=std_error_mean)

def generate_samples_df(num_samples, sample_size, df):
    sampled_data_sets = OrderedDict()

    for i in xrange(num_samples):
        print "Generating Sample", i, "..."

        sample = df.sample(sample_size)

        #Convert data sample to histogram
        sample_hist = transitions_to_histogram(sample)

        sampled_data_sets["S" + str(i)] = sample_hist

        #Reset the sample
        sample = pd.DataFrame()

    return sampled_data_sets


def generate_samples(num_samples, sample_size, df):
    sampled_data_sets = OrderedDict()

    for i in xrange(num_samples):
        print "Generating Sample", i, "..."

        sample_df = df.sample(sample_size)

        #Convert data sample to histogram
        sample_hist = transitions_to_histogram(sample_df)

        sampled_data_sets["S" + str(i)] = sample_hist

        #Reset the sample
        sample = pd.DataFrame()

    return sampled_data_sets

def plot_mean(ax, data_sets):
    means = []

    for label, data in data_sets.iteritems():
        means.append( (data['delay:MAX'] * data['probability']).sum() )

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
        maxes.append( data['delay:MAX'].values.max() )

    df = pd.DataFrame({'max_delay': maxes}).sort_values(by='max_delay')

    value_counts = df['max_delay'].value_counts()

    max_stats = pd.DataFrame({'delay:MAX': value_counts.index, 'probability': value_counts.values / float(value_counts.sum())})

    max_stats = max_stats.sort_values(by='delay:MAX')

    max_stats['cumulative_probability'] = max_stats['probability'].cumsum()


    ax.stem(max_stats['delay:MAX'], max_stats['probability'], label="Sample Max Delays (pdf)", basefmt="")

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
        extract_cmd = "cat {} 2>/dev/null | ".format(filename)


    #Determine the number of inputs in this benchmark
    header_cmd = extract_cmd + " head -1"

    result = subprocess.check_output(header_cmd, shell=True)

    cols = result.strip('\n').split(',')

    num_inputs = None
    for i, col_name in enumerate(cols):
        if col_name.startswith("delay"):
            num_inputs = i - 1 #-1 since column before delay is the output
            break

    output_name = cols[num_inputs]

    #Determine number of cases in file
    line_count_cmd = extract_cmd + " wc -l"

    result = subprocess.check_output(line_count_cmd, shell=True)

    result = result.split('\n')[0]

    num_sim_cases = int(result) - 1 #-1 for header

    return num_sim_cases, num_inputs, output_name

if __name__ == "__main__":
    main()
