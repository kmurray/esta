#!/usr/bin/env python
from collections import OrderedDict
import csv
import sys

import matplotlib.pyplot as plt

if __name__ == "__main__":

    data = OrderedDict()
    with open(sys.argv[1], 'r') as f:

        csv_reader = csv.DictReader(f)

        for row in csv_reader:
            trans = row['Transition']
            if trans not in data:
                data[trans] = []
            data[trans].append(float(row['Arrival']))

    values = data.values()
    labels = data.keys()
    n, bins, patches = plt.hist(values, label=labels, stacked=True, align='right')

    # plt.xticks(bins)

    plt.legend(loc='best')
    plt.xlabel("Delay")
    plt.ylabel("Frequency")


    plt.ylim([0,16])
    
    plt.show();
