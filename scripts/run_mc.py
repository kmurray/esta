#!/usr/bin/env python

import sys
import csv

if __name__ == "__main__":

    with open(sys.argv[1]) as f:
        reader = csv.DictReader(f)

        for row in reader:
            benchmark = row['Benchmark']
            sta_cpd = row['STA']
            mc_cpd = row['MC']
            esta_cpd = row['ESTA (no FP)']
            note = row['Notes']

            target_delay = None
            if esta_cpd != '' and note != '*':
                target_delay = esta_cpd
            else:
                target_delay = mc_cpd

            for type in ['mean', 'max']:
                cmd = ["echo 'Starting {} {}'".format(benchmark, type),
                       "cd {}.blif".format(benchmark),
                       "nice -n 19 esta_mc.py sim.trans.csv.gz --search {type} --true_max {delay} --plot_file sim_mc.max_trans.{type}.pdf >& esta_mc.{type}.log".format(type=type, delay=target_delay),
                       "echo 'Finished {} {}'".format(benchmark, type)
                        ]

                cmd_text = ' && '.join(cmd)
                cmd_text += " || echo 'Failed {} {}'".format(benchmark, type)
                print cmd_text

