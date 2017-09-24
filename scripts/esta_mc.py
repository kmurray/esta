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
    def __init__(self, msg, sample_size=None):
        super(NotConvergedException, self).__init__(msg)
        self.sample_size = sample_size

OK_EXIT_CODE = 0
ERROR_EXIT_CODE = 1
NOT_CONVERGED_EXIT_CODE = 2

def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("simulation_csv",
                        help="Simulation results CSV file")

    parser.add_argument("--keys",
                        nargs="+",
                        default=['delay:MAX'],
                        help="Which keys to check for convergence in the CSV file")

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

    parser.add_argument("--sim_time_hours",
                        default=48.0,
                        type=float,
                        help="Simulation run-time (scaled by converged sample fraction to determine MC run-time)")

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
    parser.add_argument("--input_chunk_size",
                        default=50,
                        type=int,
                        help="How many circuit inputs to load at a time (smaller values decrease peak memory usage in exchange for longer run-time")


    args = parser.parse_args()

    return args


def main():
    args = parse_args()

    print " ".join(sys.argv)

    print "Counting Cases..."
    num_sim_cases, num_inputs, output_name = inspect_sim_file(args.simulation_csv)
    print "Data file contains:", num_sim_cases, "cases"

    num_exhaustive_cases = Decimal(4)**Decimal(num_inputs)

    print "Inputs: ", num_inputs
    print "Exhaustive cases: {:g}".format(num_exhaustive_cases)

    sim_time_sec = args.sim_time_hours * 60 * 60
    TIME_THRESHOLD_SEC = 0.1
    sim_sec_per_case = sim_time_sec / num_sim_cases
    case_threshold = TIME_THRESHOLD_SEC / sim_sec_per_case
    print "Sim sec/case: {}".format(sim_sec_per_case)
    print "Sample Size Binary Search Threshold: {} ({} sec)".format(case_threshold, TIME_THRESHOLD_SEC)

    #Group keys into groups to reduce memory usage by only loading one group at a time
    ikey = -1 
    key_groups = []
    for i, key in enumerate(args.keys):

        if i % args.input_chunk_size == 0:
            ikey += 1
            key_groups.append([])

        key_groups[ikey].append(key)

    max_sample_size = None
    for key_group in key_groups:

        print "Loading delay values for", key_group
        df = pd.read_csv(args.simulation_csv, usecols=key_group)


        for key in key_group:
            print ""
            print "Checking Convergence for {}".format(key)
            print "-------------------------------------------------------"
            sample_size = None
            try:
                if args.search == 'mean':
                    sample_size, confidence_interval = search_mean(df, key, num_sim_cases, 
                                                        args.search_confidence, args.search_mean_interval_len)

                elif args.search == 'max':

                    max_delay = args.true_max
                    if max_delay is None:
                        max_delay = float(max(df[key]))
                    else:
                        max_delay = float(max_delay)

                    sample_size = search_max_prob(df[key], key, num_sim_cases,
                                    args.search_confidence, args.search_max_p_rel_interval_len, max_delay, case_threshold)
                else:
                    assert False
            except NotConvergedException as e:
                print "Not Converged"
                print e
                sample_and_plot(args, "{}.not_converged.pdf".format(key), args.num_samples, e.sample_size, df, key)
                sys.exit(NOT_CONVERGED_EXIT_CODE)

            print "Converged sample size for {}: {}".format(key, sample_size)

            sim_sample_frac = sample_size / float(num_sim_cases)
            print "MC Runtime (sec) for {}: {}".format(key, sim_sample_frac * sim_time_sec)

            if max_sample_size == None or sample_size > max_sample_size:
                max_sample_size = sample_size
                print "Max Convereged Sample Size accross keys: {} (for {})".format(max_sample_size, key)


    exhaustive_sample_frac = max_sample_size / float(num_exhaustive_cases)
    sim_sample_frac = max_sample_size / float(num_sim_cases)
    print "Final Sample Size: ", max_sample_size
    print "Final Sample Frac (simulation): ", sim_sample_frac
    print "Final Sample Frac (exhaustive): ", exhaustive_sample_frac

    print "MC Runtime (sec):", sim_sample_frac * sim_time_sec

    sys.exit(OK_EXIT_CODE)

def search_mean(df, key, num_sim_cases, search_confidence, search_mean_interval_len):
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

        mean, confidence_interval = mean_confidence_interval(sample[key], search_confidence)

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

def determine_p_est_by_interval(df, key, max_delay, search_confidence, search_max_p_rel_interval_len):
    num_sample_cases = df.shape[0]

    #Estimate probability of getting a max delay path from the entire MC sim
    num_max_delay_cases = df.value_counts()[max_delay]

    p_est = num_max_delay_cases / float(num_sample_cases)

    print "P_est: {}".format(p_est)

    #Calculate the interval to see if it has converged
    #
    #The 'beta' (Clopper-Pearson) method is a pessimistic interval which gaurentees
    #to cover the alpha-significant interval, but it may be conservative (i.e. it may
    #cover a more significant (smaller alpha)
    alpha = 1. - search_confidence
    ci = sms_sp.proportion_confint(num_max_delay_cases, num_sample_cases, alpha=alpha, method="beta")

    #Convert tuple to array
    ci = [ci[0], ci[1]]

    if max_delay == 0 and math.isnan(ci[1]):
        print "Warning: end of confidence interval was nan for max_delay 0; forcing to 1."
        ci[1] = 1.

    assert not math.isnan(ci[0])
    assert not math.isnan(ci[1])

    ci_len = ci[1] - ci[0]
    ci_len_ratio = ci_len / p_est

    print "P_est CI: [{:g}, {:g}] @ alpha={} ci_len/P_est={}".format(ci[0], ci[1], alpha, ci_len_ratio)

    if p_est < ci[0] or p_est > ci[1]:
        msg = "Estimate {:g} falls outside confidence interval [{:g}, {:g}]: NOT CONVERGED".format(p_est, ci[0], ci[1])

        raise NotConvergedException(msg, num_sample_cases)

    if ci_len_ratio > search_max_p_rel_interval_len:
        msg = "Normalized CI delta (ci[1] - ci[0])/p_ext={:g} exceeds target={:g}: NOT CONVERGED".format(ci_len_ratio, search_max_p_rel_interval_len)

        raise NotConvergedException(msg, num_sample_cases)


    return p_est, ci


def search_max_prob(df, key, num_sim_cases, search_confidence, search_max_p_rel_interval_len, max_delay, sample_size_threshold = 0):
    """
    Determines the minimal sample size for which the Monte-Carlo simulation maximum delay has converged (in the
    sense max delay having correct probability).

    We determine this by conducting a series of bernoulli (success/fail) experiments, where at different sample
    sizes
    """
    print "Specified Max delay: {}".format(max_delay)

    mc_max_delay = max(df)
    print "MC Max delay: {}".format(mc_max_delay)

    if max_delay != float(mc_max_delay):
        msg = "Max delay not found in simulation: NOT CONVERGED"
        raise NotConvergedException(msg, num_sim_cases)

    p_est, p_est_conf_interval = determine_p_est_by_interval(df, key, max_delay, search_confidence, search_max_p_rel_interval_len)

    #Binary Search for the minimum sample size with sufficient confidence
    sample_size = num_sim_cases
    lower_bound_sample_size = -1
    upper_bound_sample_size = -1
    while lower_bound_sample_size == -1 or upper_bound_sample_size == -1 or (upper_bound_sample_size - lower_bound_sample_size > sample_size_threshold):

        df_sub_sample = df.sample(sample_size)

        converged = True
        try:
            p_sub_est, p_sub_est_ci = determine_p_est_by_interval(df_sub_sample, key, max_delay, search_confidence, search_max_p_rel_interval_len)
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
            raise NotConvergedException("Failed to converge".format(p_max, lower_bound_sample_size, search_confidence), num_sim_cases)

        assert upper_bound_sample_size >= lower_bound_sample_size

        if step == 0:
            break

        sample_size += step

    assert upper_bound_sample_size != -1
    assert lower_bound_sample_size != -1
    assert upper_bound_sample_size - lower_bound_sample_size <= sample_size_threshold

    print "Minimal Sample Size: {} (@ {} confidence)".format(upper_bound_sample_size, search_confidence)

    return upper_bound_sample_size

def mean_confidence_interval(sample, confidence):
    sample_mean = sample.mean()
    std_error_mean = sp.stats.sem(sample)
    dof = len(sample) - 1
    return sample_mean, sp.stats.t.interval(confidence, dof, loc=sample_mean, scale=std_error_mean)

def sample_and_plot(args, plot_file, num_samples, sample_size, df, key):
    all_data_sets = OrderedDict()

    print "Plotting with {} samples...".format(num_samples)

    num_sim_cases = df.shape[0]
    num_indep_samples = num_sim_cases / sample_size
    if num_indep_samples < num_samples:
        new_sample_size = num_sim_cases / num_samples
        print "Warning using specified sample size {} can not generated {} independent samples; adjusting sample size to {}".format(sample_size, num_samples, new_sample_size)

        sample_size = new_sample_size

    sampled_data_sets = generate_samples(num_samples, sample_size, df, key)
    all_data_sets.update(sampled_data_sets)


    nrows = 3
    ncols = 1

    sample_frac = float(sample_size) / num_sim_cases

    fig = plt.figure(figsize=(8,8))
    fig.suptitle("{base_title} (Sample Size: {sample_size:.2g} ({sample_pct:.2g}%), Num. Samples: {num_samples})".format(base_title=args.base_title, sample_size=sample_size, sample_pct=100*sample_frac, num_samples=num_samples), fontsize=14)


    gs = GridSpec(3,1)

    cdf_ax = fig.add_subplot(gs[0,0])

    plot_ax(cdf_ax, all_data_sets, key=key, labelx=False, show_legend=False)

    cdf_ax.set_title("Delay CDF")

    cdf_xlim = cdf_ax.get_xlim()

    mean_ax = fig.add_subplot(gs[1,0])
    max_ax = fig.add_subplot(gs[2,0])

    plot_mean(mean_ax, sampled_data_sets, key=key)

    plot_max(max_ax, sampled_data_sets, key=key)

    max_ax.set_xlim(cdf_xlim)
    mean_ax.set_xlim(cdf_xlim)

    if args.plot_file == "interactive":
        plt.show()
    else:
        plt.savefig(plot_file)


def generate_samples(num_samples, sample_size, df, key):
    sampled_data_sets = OrderedDict()

    for i in xrange(num_samples):
        print "Generating Sample", i, " of size", sample_size, "..."

        sample_df = df.sample(sample_size)

        #Convert data sample to histogram
        sample_hist = transitions_to_histogram(sample_df, key=key)

        sampled_data_sets["S" + str(i)] = sample_hist

        #Reset the sample
        sample = pd.DataFrame()

    return sampled_data_sets

def plot_mean(ax, data_sets, key):
    means = []

    for label, data in data_sets.iteritems():
        means.append( (data[key] * data['probability']).sum() )

    df = pd.DataFrame({'mean_delay': means}).sort_values(by='mean_delay')

    df.hist(ax=ax, bins=15)

    ax.set_ylabel("Sample Count")

    ax.set_title("Mean Delay")

    #value_counts = df['mean_delay'].value_counts()
    #ax.stem(value_counts.index, value_counts.values)

    #print df.sort_values(by='mean_delay')

def plot_max(ax, sampled_data_sets, key, true_max=None):
    maxes = []

    for label, data in sampled_data_sets.iteritems():
        maxes.append( data[key].values.max() )

    df = pd.DataFrame({'max_delay': maxes}).sort_values(by='max_delay')

    value_counts = df['max_delay'].value_counts()

    max_stats = pd.DataFrame({key: value_counts.index, 'probability': value_counts.values / float(value_counts.sum())})

    max_stats = max_stats.sort_values(by=key)

    max_stats['cumulative_probability'] = max_stats['probability'].cumsum()


    ax.stem(max_stats[key], max_stats['probability'], label="Sample Max Delays (pdf)", basefmt="")

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
