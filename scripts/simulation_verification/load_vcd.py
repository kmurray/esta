#!/usr/bin/env python
import argparse
from Verilog_VCD import parse_vcd, list_sigs

from itertools import product
from bisect import bisect_left
from collections import OrderedDict
import sys
import pudb

class LogicValue:
    High = "1"
    Low = "0"
    Unkown = "X"

class TransType:
    Rise = "R"
    Fall = "F"
    High = "H"
    Low = "L"

class CycleTransitions(object):
    __slots__ = ['launch_edge_', 'input_transitions_', 'output_transitions_', 'output_delays_']

    def __init__(self, launch_edge, input_transitions, output_transitions, output_delays):
        self.launch_edge_ = launch_edge
        self.input_transitions_ = input_transitions
        self.output_transitions_ = output_transitions
        self.output_delays_ = output_delays

    def launch_edge(self):
        return self.launch_edge_

    def output_delay(self, output_name):
        return self.output_delays_[output_name]

    def input_transitions(self):
        return self.input_transitions_;

    def output_transition(self, output_name):
        return self.output_transitions_[output_name]

    def __str__(self):
        val = "@{edge}".format(edge=self.launch_edge())
        for input, trans in self.input_transitions().iteritems():
            val += " {input}: {trans}".format(input=input, trans=trans)
        for output in self.output_transitions_.keys():
            val += " {out}: {trans} {delay}".format(out=output, trans=self.output_transition(output), delay=self.output_delay(output))

        return val


def main():
    args = parse_args()

    print "Parsing VCD"
    vcd = load_vcd(args)

    print "Extracting Cycle data"
    cycle_data = vcd_to_cycle_data(vcd, clock=args.clock, inputs=args.inputs, outputs=[args.output], exhaustive=args.exhaustive)

    #for cycle_trans in cycle_data:
        #print cycle_trans

    print "Uniqifying input transitions"
    unique_transitions = uniquify_input_transitions(args, cycle_data)

    print "Unique transitions: ", len(unique_transitions)
    seen_transition_count = 0
    for key, value in unique_transitions.iteritems():
        if value == None:
            #print "{key} : Missing".format(key=key)
            pass
        else:
            seen_transition_count += 1
            prob = value[2] / float(len(cycle_data))
            #print "{key} : {trans} {delay} {prob:.4f}".format(key=key, trans=value[0], delay=value[1], prob=prob)

    missing_transition_frac = 1. - float(seen_transition_count) / len(unique_transitions)
    print "Missing {num} ({frac}) transitions".format(num=len(unique_transitions) - seen_transition_count, frac=missing_transition_frac)

    print "Saw {num} transitions".format(num=seen_transition_count)

    if args.csv_out:
        print "Writting CSV"
        with open(args.csv_out, 'w') as f:
            #header
            header_vals = args.inputs + [args.output] + ['delay', 'exact_prob', 'measured_prob', 'sim_time']
            print >>f, ','.join(header_vals)

            #Rows
            for key, value in unique_transitions.iteritems():
                row = [x for x in key]
                if value:
                    row += [value[0], str(value[1]), str(1./4**len(args.inputs)), str(value[2]/4**len(args.inputs)), str(value[3])]
                else:
                    row += ["", "", str(1./4**len(args.inputs)), str(0), str(-1)]
                print >>f, ",".join(row)

    #print build_historgram('o_c', trans_delay['o_c'])

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("--vcd_file", "-v",
                        required=True,
                        help="VCD file")

    parser.add_argument("--inputs", "-i",
                        nargs="+",
                        required=True,
                        help="Input signal names")

    parser.add_argument("--output", "-o",
                        required=True,
                        help="Output signal name")

    parser.add_argument("--clock", "-c",
                       default="clk",
                       help="Clock name. Default: %(default)s")

    parser.add_argument("--csv_out",
                        help="CSV output file")
    parser.add_argument("--exhaustive",
                        action="store_true",
                        default=False,
                        help="Wheter performing an exhaustive analysis which assume launch = clock rise, and capture = clock fall")

    args = parser.parse_args()

    return args

def load_vcd(args):
    #Determine the signals we want to keep
    sigs = list_sigs(args.vcd_file)

    base_sig_names = args.inputs + [args.clock] + [args.output]

    signal_list = []
    for sig in sigs:
        for base_sig_name in base_sig_names:
            if base_sig_name in sig:
                signal_list.append(sig)

    vcd = parse_vcd(args.vcd_file, siglist=signal_list)

    return vcd

def build_historgram(net, trans_delay):
    count = {TransType.Rise: {}, TransType.Fall: {}, TransType.High: {}, TransType.Low: {}}
    total_count = len(trans_delay)

    #Accumulate
    for transition in trans_delay:
        if transition.time() not in count[transition.type()]:
            count[transition.type()][transition.time()] = 0.

        count[transition.type()][transition.time()] += 1.

    #Normalize
    for transition_key in count.keys():
        for delay_key in count[transition_key].keys():
            count[transition_key][delay_key] /= total_count

    return count

def uniquify_input_transitions(args, cycle_data):
    #Uniqify on input transitions
    #unique_transitions = OrderedDict()
    #for a_trans in [TransType.Rise, TransType.Fall, TransType.High, TransType.Low]:
        #for b_trans in [TransType.Rise, TransType.Fall, TransType.High, TransType.Low]:
            #unique_transitions[(a_trans, b_trans)] = None

    unique_transitions = init_uniq_transitions(args)

    for cycle_trans in cycle_data:
        input_trans = []
        for input_name in args.inputs:
            input_trans.append(cycle_trans.input_transitions()[input_name])
        input_trans = tuple(input_trans)
        output_trans = cycle_trans.output_transition(args.output)
        output_delay = cycle_trans.output_delay(args.output) 
        sim_time = cycle_trans.launch_edge() 
        if unique_transitions[input_trans] == None:
            #First time this input transition is seen
            unique_transitions[input_trans] = output_trans, output_delay, 1, sim_time
        else:
            #Seen again, 
            assert unique_transitions[input_trans][0] == output_trans, "Missmatched output transition for input: " + str(input_trans) + " expected: " + str(unique_transitions[input_trans][0]) + " was: " + str(output_trans) + " at time " + str(cycle_trans.launch_edge())
            assert unique_transitions[input_trans][1] == output_delay, "Missmatched output delay for input: " + str(input_trans) + " expected: " + str(unique_transitions[input_trans][1]) + " was: " + str(output_delay) + " at time " + str(cycle_trans.launch_edge())


            #unique_transitions[input_trans][2] += 1
            unique_transitions[input_trans] = (output_trans, output_delay, unique_transitions[input_trans][2] + 1, sim_time)

    return unique_transitions

def init_uniq_transitions(args):
    valid_transitions = [TransType.Rise, TransType.Fall, TransType.High, TransType.Low]

    unique_transitions = OrderedDict()
    for i, input_transitions in enumerate(product(valid_transitions, repeat=len(args.inputs))):
        if i % 1e6 == 0:
            print "Initialized ", i, " input scenarios", float(i) / len(valid_transitions)**len(args.inputs)
        unique_transitions[input_transitions] = None

    assert len(unique_transitions) == 4**len(args.inputs)
    return unique_transitions

def vcd_to_cycle_data(vcd, clock, inputs, outputs, exhaustive=False):
    cycle_data = []
    #
    #Convert to directl hash on time values
    #
    data_times = {}
    data_values = {}
    rise_clk_edges = None
    fall_clk_edges = None
    for symbol, data in vcd.iteritems():
        assert 'tv' in data
        assert 'nets' in data

        assert len(data['nets']) == 1

        if data['nets'][0]['type'] == 'parameter': continue

        net_name = data['nets'][0]['name']
        if net_name == clock:
            rise_clk_edges, fall_clk_edges = extract_clock_sample_times(data['tv'])
        else:
            start_index = 0 #Skip first values due to unkowns

            #Extract the time and values separately
            times = [x[0] for x in data['tv'][start_index:]]
            values = [str_val_to_logic(x[1]) for x in data['tv'][start_index:]]

            #Save them
            data_times[net_name] = times
            data_values[net_name] = values

    #
    #Extract data from each cycle
    #
    for i in xrange(len(rise_clk_edges) - 1):

        launch_edge = rise_clk_edges[i]

        if exhaustive:
            #In exhaustive simulation we assume the data is stable by
            #the falling edge of the clock, and allow the later-half of
            #the cycle to be setup for the next case (activated at next
            #rising edge)
            capture_edge = fall_clk_edges[i+1]
        else:
            #If non-exhaustive (i.e. randomized), we assume
            #the capture is the next rising clock edge
            capture_edge = rise_clk_edges[i+1]

        assert launch_edge < capture_edge

        input_transitions = {}

        for input in inputs:
            input_times = data_times[input]
            input_values = data_values[input]

            i_prev = find_index_lt(input_times, launch_edge)

            try:
                i_next = find_index_ge(input_times, launch_edge)
            except ValueError:
                #No transition after the launch edge (e.g. constant till end of simulation)
                i_next = i_prev

            #Adjust if the next transition occurs outside the current cycle
            # i.e. there was not input transition
            if input_times[i_next] >= capture_edge:
                i_next = i_prev


            input_transitions[input] = prev_next_to_trans(input_values[i_prev], input_values[i_next])

        output_transitions = {}
        output_delays = {}

        for output in outputs:
            output_times = data_times[output]
            output_values = data_values[output]

            i_prev = find_index_lt(output_times, launch_edge)
            i_final = find_index_lt(output_times, capture_edge)

            output_transitions[output] = prev_next_to_trans(output_values[i_prev], output_values[i_final])

            output_stable_time = max(launch_edge, output_times[i_final])

            output_delay = output_stable_time - launch_edge

            output_delays[output] = output_delay

        cycle_data.append( CycleTransitions(launch_edge, input_transitions, output_transitions, output_delays) )

    return cycle_data 

def str_val_to_logic(str_val):
    if str_val == "1":
        return LogicValue.High
    elif str_val == "0":
        return LogicValue.Low
    else:
        assert str_val == "x"
        return LogicValue.Unkown

def prev_next_to_trans(prev, next):
    if prev == LogicValue.Low and next == LogicValue.High:
        return TransType.Rise
    elif prev == LogicValue.Low and next == LogicValue.Low:
        return TransType.Low
    elif prev == LogicValue.High and next == LogicValue.Low:
        return TransType.Fall
    elif prev == LogicValue.High and next == LogicValue.High:
        return TransType.High
    elif prev == LogicValue.Unkown and next == LogicValue.High:
        return TransType.Rise
    elif prev == LogicValue.Unkown and next == LogicValue.Low:
        return TransType.Fall
    else:
        assert False

def transition_final_logic_value(transition_type):
    if transition_type in [TransType.High, TransType.Rise]:
        return LogicValue.High
    elif transition_type in [TransType.Low, TransType.Fall]:
        return LogicValue.Low
    assert False

def transition_initial_logic_value(transition_type):
    if transition_type in [TransType.High, TransType.Fall]:
        return LogicValue.High
    elif transition_type in [TransType.Low, TransType.Rise]:
        return LogicValue.Low
    assert False

def find_cycle_transitions(transitions, transition_times, launch_edge, capture_edge):
    start_index = find_index_ge(transition_times, launch_edge)
    end_index = find_index_lt(transition_times, capture_edge)

    return transitions[start_index:end_index+1]

def find_last_transition_before_edge(transitions, transition_times, edge):
    index = find_index_lt(transition_times, edge)

    return transitions[index]

def find_index_ge(a, x):
    'Find leftmost index greater than or equal to x'
    i = bisect_left(a, x)
    if i != len(a):
        return i
    raise ValueError

def find_index_lt(a, x):
    'Find rightmost index less than x'
    i = bisect_left(a, x)
    if i:
        return i-1
    raise ValueError

def extract_clock_sample_times(time_values):
    rise_edges = []
    fall_edges = []

    prev_tv = time_values[0]
    for next_tv in time_values[1:]:
        prev_time = prev_tv[0]
        prev_value = prev_tv[1]

        next_time = next_tv[0]
        next_value = next_tv[1]

        trans_type = prev_next_to_trans(prev_value, next_value)
        if trans_type == TransType.Rise:
            rise_edges.append(next_time)
        elif trans_type == TransType.Fall:
            fall_edges.append(next_time)

        prev_tv = next_tv

    return rise_edges, fall_edges 

def print_vcd(vcd):
    for symbol, data in vcd.iteritems():
        assert 'tv' in data
        assert 'nets' in data

        assert len(data['nets']) == 1

        net_info = data['nets'][0]

        print "Net: ", net_info['hier'], net_info['name']

        print "\tTime Values:"
        for time, value in data['tv']:
            print "\t", time, value

if __name__ == "__main__":
    main()
