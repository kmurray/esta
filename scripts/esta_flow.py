#!/usr/bin/env python
import os
import sys
import argparse
import subprocess
import shutil
import re
import fnmatch
import json
import numpy as np
import math
import time
import glob

import pyverilog.vparser.parser as verilog_parser
import pyverilog.vparser.ast as vast
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator

from esta_util import CommandError, CommandRunner
from esta_util import transitions_to_histogram as exhaustive_csv_to_histogram_csv

SIM_CLOCK="sim_clk"


def parse_args():
    parser = argparse.ArgumentParser(
                formatter_class=argparse.ArgumentDefaultsHelpFormatter
            )

    #
    # General arguments
    #
    parser.add_argument("arch",
                        help="VTR FPGA Architecture description")

    parser.add_argument("blif",
                        help="Circuit BLIF file")

    parser.add_argument("--vtr",
                        action="store_true",
                        dest="run_vtr",
                        default=False,
                        help="Run VTR to produce SDF back-annotation")

    parser.add_argument("--esta",
                        action="store_true",
                        dest="run_esta",
                        default=False,
                        help="Run ESTA with SDF back-annotation")

    parser.add_argument("--sim",
                        action="store_true",
                        dest="run_sim",
                        default=False,
                        help="Run Simulation wtih SDF back-annotation")

    parser.add_argument("--sim_extract",
                        action="store_true",
                        dest="run_sim_extract",
                        default=False,
                        help="Extract transition and delay data from simulation result")

    parser.add_argument("--compare",
                        action="store_true",
                        dest="run_compare",
                        default=False,
                        help="Run comparison between Simulation and ESTA for verification.")

    parser.add_argument("--plot",
                        action="store_true",
                        dest="run_plot",
                        default=False,
                        help="Plot the comparison between Simulation and ESTA")

    parser.add_argument("--outputs",
                        nargs="*",
                        default=None,
                        help="Outputs to compare")

    parser.add_argument("-d", "--delay_bin_size_coarse",
                        default=100000,
                        help="Delay bin size for ESTA. Smaller values increase accuracy at the cost of longer run-time.")

    parser.add_argument("-f", "--delay_bin_size_fine",
                        default=0,
                        help="Delay bin size for ESTA. Smaller values increase accuracy at the cost of longer run-time.")

    parser.add_argument("-m", "--max_permutations",
                        default=1e7,
                        help="Maximum number of permutations (per node) for ESTA. Smaller values reduce runtime at the cost of lower accuracy. A value of 0 causes no limit to be enforced.")

    parser.add_argument("-s", "--slack_ratio",
                        default=0.00,
                        help="Percentile slack to never reduce")

    parser.add_argument("--sim_mode",
                        choices=["exhaustive", "monte_carlo"],
                        default="monte_carlo",
                        help="Simulation mode.")

    parser.add_argument("--monte_carlo_iter_fraction",
                        type=float,
                        default=0.1,
                        help="Sets the fraction of input space to cover during monte carlo simulation. This sets the number of monte carlo iterations to a fraction of the exhaustive iteration count")

    parser.add_argument("-v", "--verbose",
                        default=False,
                        action="store_true",
                        help="Produce more verbose console output.")

    #
    # VTR related arguments
    #
    vtr_arguments = parser.add_argument_group("VTR", "VTR related options")
    vtr_arguments.add_argument("--vpr_exec",
                               default="vpr",
                               help="VTR root directory")
    #
    # ESTA related arguments
    #
    esta_arguments = parser.add_argument_group("ESTA", "ESTA related options")
    esta_arguments.add_argument("--reorder_method",
                                default=None,
                                help="BDD reordering method.")
    esta_arguments.add_argument("--esta_exec",
                                default="esta",
                                help="ESTA executable.")
    esta_arguments.add_argument("--esta_mode",
                                choices=['max', 'po'],
                                default="max",
                                help="ESTA executable.")

    #
    # Modelsim related arguments
    #
    modelsim_arguments = parser.add_argument_group("Modelsim", "Modelsim related options")
    modelsim_arguments.add_argument("--vsim_exec",
                                    default="vsim",
                                    help="Modelsim vsim executable.")

    modelsim_arguments.add_argument("--modelsim_flavour",
                                    choices=['modelsim', 'modelsim_altera'],
                                    default='modelsim',
                                    help="Adjust modlesim scripts if running vanilla modelsim (no adjustments) or altera modelsim (Include Altera primitives and Fix bug with Modelsim-Altera where it will not allow SDF annotation without an instantiated Altera primitive)")

    modelsim_arguments.add_argument("--link_verilog",
                                    nargs="+",
                                    default=['/project/trees/vtr/vtr_flow/primitives.v'],
                                    help="Extra verilog files to include in modelsim simulation (e.g. VTR's primitives.v)")

    modelsim_arguments.add_argument("--cpd_scale",
                                    default=1.3,
                                    type=float,
                                    help="Factor to expand critical path delay by to set modelsim clock period. Should be >= 1.0.")
    #
    # VCD Extraction arguments
    #
    transition_extraction_arguments = parser.add_argument_group("Transition Extraction", "Options for processing simulation VCD")
    transition_extraction_arguments.add_argument("--vcd_extract_exec",
                                                 default="vcd_extract",
                                                 help="Tool used to post-process VCD to extract transitions and delays.")
    transition_extraction_arguments.add_argument("--vcd_split_exec",
                                                 default="vcd_split",
                                                 help="Tool used to split VCD files into chunks.")
    transition_extraction_arguments.add_argument("--vcd_output_dir",
                                                 default=None,
                                                 help="Directory to write extraction results. None implies the current directory.")
    transition_extraction_arguments.add_argument("--vcd_size_limit",
                                                 default=None,
                                                 help="Maximum VCD file size (in bytes) before it is split into chunks. Downstream tools currently read the whole VCD file into memory -- chunking limits their memory usage")

    #
    # Comparison/verification related arguments
    #
    comparison_arguments = parser.add_argument_group("Comparision and Verification", "Options for comparing the ESTA and Simulation results")
    comparison_arguments.add_argument("--comparison_exec",
                                      default="compare_exhaustive.py",
                                      help="Tool used compare ESTA and simulation results.")
    #
    # Plotting related arguments
    #
    plot_arguments = parser.add_argument_group("Plotting", "Options for plotting the ESTA and Simulation results")
    plot_arguments.add_argument("--plot_exec",
                                default="esta_plot.py",
                                help="Tool used compare ESTA and simulation results.")

    args = parser.parse_args()

    #If none of the specific steps are specified, run all steps
    if not args.run_vtr and not args.run_esta and not args.run_sim and not args.run_sim_extract and not args.run_compare and not args.run_plot:
        args.run_vtr = True
        args.run_esta = True
        args.run_sim = True
        args.run_sim_extract = True
        args.run_compare = True
        args.run_plot = True

    if args.vcd_size_limit != None:
        args.vcd_size_limit = int(args.vcd_size_limit)

    return args

def main():
    args = parse_args()
    try:
        esta_flow(args)
    except subprocess.CalledProcessError as e:
        print "Error command failed (non-zero exit-code", e.returncode, "): ", e.cmd
        sys.exit(1)
    sys.exit(0)

def esta_flow(args):
    vpr_log = "vpr.log"
    vcd_file = "sim.vcd"

    vpr_cpd_ps = None
    endpoint_timing = None

    if args.run_vtr:
        #Run VTR to generate an SDF file
        print
        print "Running VTR"
        vtr_results = run_vtr(args, vpr_log)

    #Extract port and top instance information
    print
    print "Extracting design information"
    post_synth_blif, post_synth_verilog, post_synth_sdf = find_post_synth()
    design_info = extract_design_info(post_synth_verilog, args.blif)


    #Run ESTA
    if args.run_esta:
        print
        print "Running ESTA"
        esta_results = run_esta(args, design_info=design_info, sdf_file=post_synth_sdf)


    #Run Modelsim to collect simulation statistics
    if args.run_sim:
        print
        print "Extracting STA endpoint timing"
        vpr_cpd_ps = parse_vpr_cpd(vpr_log)


        print
        print "Running Modelsim"
        modelsim_results = run_modelsim(args,
                                        sdf_file=post_synth_sdf,
                                        cpd_ps=vpr_cpd_ps,
                                        verilog_info=design_info,
                                        vcd_file=vcd_file
                                        )

    if args.run_sim or args.run_sim_extract:
        print
        print "Extracting Transitions"
        transition_results = run_transition_extraction(args, vcd_file, design_info)

    if args.run_compare and args.sim_mode == "exhaustive":
        print
        print "Comparing ESTA and Simulation Exhaustive Results"
        run_comparison(args, design_info)

    if args.run_plot:
        print
        print "Loading STA Endpoint Timing"
        endpoint_timing = load_endpoint_timing()

        if vpr_cpd_ps != None:
            assert np.isclose(vpr_cpd_ps, max(endpoint_timing.values()))

        print
        print "Plotting ESTA and Simulation Results"
        run_plot(args, design_info, endpoint_timing)

def run_vtr(args, vpr_log_filename):

    #Localize the blif file, since VPR likes to put its files next to it
    orig_blif_location = os.path.abspath(args.blif)
    new_blif_location = os.path.join(os.getcwd(), os.path.basename(orig_blif_location))
    if orig_blif_location != new_blif_location:
        shutil.copy(orig_blif_location, new_blif_location)

    args.blif = new_blif_location
    cmd = [
            args.vpr_exec,
            args.arch,
            args.blif,
            "--absorb_buffer_luts", "off",
            "--sweep_dangling_primary_ios", "off",
            "--sweep_dangling_nets", "off",
            "--sweep_dangling_blocks", "off",
            "--route_chan_width", "300",
            "--echo_file", "on",
            "--gen_post_synthesis_netlist", "on",
            "--disp", "off"
          ]

    output = run_command(cmd, log_filename=vpr_log_filename, verbose=args.verbose)

    return {
            'post_synth_verilog': 'top_post_synthesis.v',
            'post_synth_sdf': 'top_post_synthesis.sdf',
           }

def parse_vpr_cpd(vpr_log_filename):

    cpd_ns_regex = re.compile(r".*Final critical path: (?P<cpd_ns>[\d.]+) ns.*", re.DOTALL)

    with open(vpr_log_filename) as f:
        match = None
        for line in f:
            line = line.strip()
            match = cpd_ns_regex.match(line)
            if match:
                break

        assert match, "Could not find VPR's critical path delay"

        cpd_ns = float(match.group("cpd_ns"))

    cpd_ps = cpd_ns*1000

    return cpd_ps


def run_esta(args, design_info, sdf_file):
    if args.outputs == None:
        dump_outputs = design_info['outputs']
    else:
        dump_outputs = args.outputs


    cmd = [
            args.esta_exec,
            "-b", args.blif,
            "-s", sdf_file,
            "-d", args.delay_bin_size_coarse,
            "-f", args.delay_bin_size_fine,
            "-m", args.max_permutations,
            "--slack_threshold", args.slack_ratio
            ]

    if args.esta_mode == "max":
        pass
    elif args.esta_mode == "po":
        cmd += ['--max_histogram', 'false']
        cmd += ['--print_histograms', 'po']
    else:
        raise ValueError("Unrecognized esta mode {}".format(args.esta_mode))

    if args.reorder_method is not None:
        cmd += ["--reorder_method", args.reorder_method]

    if args.sim_mode == "exhaustive":
        cmd += ["--max_exhaustive"]
        if len(dump_outputs) > 0 and not (len(dump_outputs) == 1 and dump_outputs[0] == ""):
            cmd += ["--dump_exhaustive_csv", ",".join(dump_outputs)]

    run_command(cmd, verbose=args.verbose)

def run_modelsim(args, sdf_file, cpd_ps, verilog_info, vcd_file):
    top_verilog = verilog_info["file"]

    #Fix the input verilog for Modelsim-Altera if required
    if args.modelsim_flavour == "modelsim_altera":
        modelsim_top_verilog = fix_modelsim_altera_sdf_annotation(top_verilog)
    else:
        modelsim_top_verilog = top_verilog


    tb_verilog = "tb.sv"
    sim_do_file = "tb.do"

    #Write out the test bench
    with open(tb_verilog, "w") as f:
        for line in create_testbench(args,
                                     sdf_file=sdf_file,
                                     critical_path_delay_ps=cpd_ps,
                                     top_module=verilog_info['module'],
                                     dut_inputs=verilog_info['inputs'],
                                     dut_outputs=verilog_info['outputs'],
                                     dut_clocks=verilog_info['clocks']):
            print >>f, line

    #Write out the modelsim .do file
    with open(sim_do_file, "w") as f:
        for line in create_modelsim_do(args,
                                       vcd_file,
                                       [modelsim_top_verilog, tb_verilog] + args.link_verilog,
                                       dut_inputs=verilog_info['inputs'],
                                       dut_outputs=verilog_info['outputs'],
                                       dut_clocks=verilog_info['clocks']):
            print >>f, line

    #Run the simulation
    cmd = [
            args.vsim_exec,
            "-c",
            "-do", sim_do_file
          ]

    run_command(cmd, verbose=args.verbose)

    return {
            'vcd_file': vcd_file,
           }

def run_transition_extraction(args, raw_vcd_file, top_verilog_info):
    """
    Extracts transition scenarios from a VCD file.

    If vcd_size_limit is specified, and the vcd file is greater than its value,
    the VCD file is split into chunks and processed seperately, with each
    chunk's results being appended to the output CSVs.
    """

    if not os.path.isfile(raw_vcd_file):
        if os.path.isfile(raw_vcd_file + ".gz"):
            vcd_file = raw_vcd_file + ".gz"
        else:
            assert false, "Could not find VCD file {}".format(vcd_file)
    else:
        vcd_file = raw_vcd_file
    
    if args.outputs is None:
        outputs = top_verilog_info['outputs']
    else:
        outputs = args.outputs

    base_args = ['-c', SIM_CLOCK]

    #Write out the inputs/outputs to a text file (avoids arguments too long)
    with open("inputs.txt", "w") as f:
        for input in top_verilog_info['inputs']:
            print >>f, input

    with open("outputs.txt", "w") as f:
        for output in outputs:
            print >>f, output

    base_args += ["-i", "inputs.txt"]
    base_args += ["-o", "outputs.txt"]

    if args.vcd_output_dir:
        base_args += ["--output_dir", args.vcd_output_dir]

    cmd = [args.vcd_extract_exec, "-v", vcd_file] + base_args

    run_command(cmd, verbose=args.verbose)

    # #Find all the produced transition CSVs
    # output_path = args.vcd_output_dir if args.vcd_output_dir != None else "."
    # trans_csvs = []
    # for exhaustive_csv in os.listdir(output_path):
        # if fnmatch.fnmatch(exhaustive_csv, "sim.trans.*.csv"):
            # trans_csvs.append(exhaustive_csv)

    # #Convert them to histograms
    # for in_csv in trans_csvs:
        # histogram_csv = ".".join(['sim', 'hist'] + exhaustive_csv.split('.')[1:])
        # exhaustive_csv_to_histogram_csv(in_csv, histogram_csv)

    return {}

def run_vcd_split(args, vcd_file, vcd_size_limit):
    """
    Splits a VCD file into chunks based on the vcd_size_limit and its size

    Returns a list of the resulting files
    """
    cmd = [
            args.vcd_split_exec,
            vcd_file,
            vcd_size_limit
            ]

    output = run_command(cmd, verbose=args.verbose)

    #Collect the result filenames
    split_files = []
    for line in output:
        if line.startswith("Writting"):
            filename = line.split()[1]

            split_files.append(filename)

    return split_files

def run_comparison(args, design_info):
    if args.outputs is None:
        outputs = design_info['outputs']
    else:
        outputs = args.outputs

    #Per output comparisions
    for output in outputs + ["MAX"]:
        print "Comparing: {port}".format(port=output)

        sim_csv = pick_sim_csv(output, "trans")

        esta_csv = pick_esta_csv(output, "trans")

        cmd = [
                args.comparison_exec,
                sim_csv,
                esta_csv,
              ]

        run_command(cmd, log_filename='.'.join(["comparison", output, "log"]), verbose=args.verbose)

    return {}

def run_plot(args, design_info, endpoint_timing):
    if args.outputs is None:
        outputs = design_info['outputs']
    else:
        outputs = args.outputs

    for output in outputs:
        print "Plotting output: {port}".format(port=output)

        sim_csv = pick_sim_csv(output, "trans")

        cmd = [args.plot_exec]

        if args.sim_mode == "exhaustive":
            esta_exhaustive_csv = pick_esta_csv(output, "trans")

            cmd += ["--exhaustive_csvs", sim_csv, esta_exhaustive_csv]
            cmd += ["--exhaustive_csv_labels", 'SIM_EXHAUSTIVE', "ESTA"]
        else:
            assert args.sim_mode == "monte_carlo"

            esta_histogram_csv = pick_esta_csv(output, "hist")

            #Sim is always treated as exhaustive
            if os.path.exists(sim_csv):
                cmd += ["--exhaustive_csvs", sim_csv]
                cmd += ["--exhaustive_csv_labels", 'SIM_MC']

            #But now we use the esta histogram directly
            cmd += ["--histogram_csvs", esta_histogram_csv]
            cmd += ["--histogram_csv_labels", "ESTA"]

        cmd += ["--sta_cpd", endpoint_timing[output]]
        cmd += ["--plot_title", '{}: {}'.format(os.path.splitext(os.path.basename(args.blif))[0], output)]
        cmd += ["--plot_file", output + ".pdf"]
        #cmd += ["--plot", "stem_cdf"]

        run_command(cmd, log_filename='.'.join(["comparison", output, "log"]), verbose=args.verbose)

    return {}

def pick_sim_csv(output, type):
    if output == "MAX":
        sim_csv = ".".join(["sim", "max_" + type, "csv"])
    else:
        sim_csv = ".".join(["sim", type, output, "csv"])
    if not os.path.isfile(sim_csv):
        sim_csv = ".".join([sim_csv, "gz"])
        assert os.path.isfile(sim_csv), "Could not find sim CSV file for port {}".format(output)

    return sim_csv

def pick_esta_csv(output, type):
    if output == "MAX":
        esta_csv = "esta.max_" + type + ".csv"
        assert os.path.isfile(esta_csv), "Could not find file " + esta_csv
        return esta_csv
    else:
        esta_csv_regex = re.compile(r"esta\." + type + "\." + output + "\.(?P<node_text>n\d+).*\.csv(.gz)?")
    #Collect all the esta exhaustive csvs
    esta_csvs = []
    for filename in os.listdir(os.getcwd()):
        match = esta_csv_regex.match(filename)
        if match:
            esta_csvs.append((filename, int(match.group("node_text")[1:])))

    assert len(esta_csvs) > 0

    #In descending order
    esta_csvs = sorted(esta_csvs, key=lambda x: x[1], reverse=True)

    #Only analyze the csv with the highest node number,
    #this is a fragile assumption based on how esta constructs the timing graph
    #the highest node number corresponds to the OUTPAD_SINK
    esta_csv = esta_csvs[0][0]

    return esta_csv

def fix_modelsim_altera_sdf_annotation(top_verilog):
    base, ext = os.path.splitext(top_verilog)
    new_file = '.'.join([base, "fixed"]) + ext;

    #
    #Load the original verilog
    #
    ast, directives = verilog_parser.parse([top_verilog])

    #
    #Add an unused stratixiv cell
    #
    mod_def = ast.description.definitions[0] #Definition of top

    new_inst = vast.Instance('stratixiv_io_ibuf', 'fix_modelsim_altera_sdf_annotation', [], []);
    new_inst_list = vast.InstanceList('stratixiv_io_ibuf', [], [new_inst])

    items = list(mod_def.items) + [new_inst_list]

    new_mod_def = vast.ModuleDef(mod_def.name, mod_def.paramlist, mod_def.portlist, items)

    #
    #Write out the new verilog
    #
    codegen = ASTCodeGenerator()
    with open(new_file, "w") as f:
        print >>f, codegen.visit(new_mod_def)

    return new_file

def extract_design_info(top_verilog, blif_file):
    #
    #Load the original verilog
    #
    ast, directives = verilog_parser.parse([top_verilog])

    mod_def = ast.description.definitions[0] #Definition of top

    #Get the module name
    name = mod_def.name

    #
    # For verification purposes we want the inputs to be specified
    # in the same order as the input blif file.
    #
    # To achieve this we quickly grab the inputs from the blif and use
    # that order.  Otherwise VPR internally changes the order which
    # makes verification difficult.
    #
    inputs = None
    outputs = None
    with open(blif_file) as f:
        for line in continued_lines(f):
            line.strip()
            if line.startswith(".inputs"):
                inputs = line.split()[1:]
            if line.startswith(".outputs"):
                outputs = line.split()[1:]
            if inputs and outputs:
                break
    assert inputs
    assert outputs

    clock_nets = set()
    while True:
        old_clock_nets = clock_nets.copy()
        clock_nets = identify_clock_nets(mod_def, clock_nets)

        if clock_nets == old_clock_nets:
            break
        else:
            old_clock_nets = clock_nets

    #Remove any clocks that show up in the input list
    clocks = [x for x in inputs if x in clock_nets]
    inputs = [x for x in inputs if x not in clock_nets]



    return {
            'file': top_verilog,
            'module': name,
            'inputs': inputs,
            'outputs': outputs,
            'clocks': clocks,
           }

def continued_lines(f):
    for line in f:
        line = line.rstrip('\n')
        while line.endswith('\\'):
            line = line[:-1] + next(f).rstrip('\n')
        yield line

def identify_internal_clock_nets(vnode):
    clocks = set()
    if isinstance(vnode, vast.InstanceList) and vnode.module == "DFF":
        ports = find_inst_list_ports(vnode)

        clocks |= set([x.argname for x in ports if x.portname == "clock"])
    else:
        for child in vnode.children():
            clocks |= identify_internal_clock_nets(child)
    return clocks

def find_inst_list_ports(vnode):
    ports = []

    if isinstance(vnode, vast.PortArg):
        ports.append(vnode)
    else:
        for child in vnode.children():
            ports += find_inst_list_ports(child)

    return ports

def identify_clock_nets(vnode, clock_nets):
    if isinstance(vnode, vast.InstanceList) and vnode.module == "DFF":
        ports = find_inst_list_ports(vnode)

        clock_nets |= set([str(x.argname) for x in ports if x.portname == "clock"])
    elif isinstance(vnode, vast.InstanceList) and vnode.module == "fpga_interconnect":
        ports = find_inst_list_ports(vnode)

        assert len(ports) == 2

        port_in = ports[0]
        port_out = ports[1]

        if str(port_out.argname) in clock_nets:
            clock_nets.add(str(port_in.argname))

    elif isinstance(vnode, vast.Assign):
        if str(vnode.left.var) in clock_nets:
            clock_nets.add(str(vnode.right.var))
    else:
        #Recurse
        for child in vnode.children():
            clock_nets |= identify_clock_nets(child, clock_nets)

    return clock_nets


def load_endpoint_timing():
    endpoint_timing = {}
    with open("endpoint_timing.echo.json") as f:
        data = json.load(f)

        for entry in data['endpoint_timing']:
            endpoint = entry['node_identifier']
            arrival_time = float(entry['T_arr']) * 1e12 #Convert to ps

            endpoint_timing[endpoint] = arrival_time

    return endpoint_timing

def find_post_synth():
    blif_matches = glob.glob("*post_synthesis.blif")
    sdf_matches = glob.glob("*post_synthesis.sdf")
    verilog_matches = glob.glob("*post_synthesis.v")

    if len(blif_matches) == 0:
        blif_matches.append(None)

    assert len(blif_matches) == 1, "Found multiple post synthesis BLIF files"
    assert len(sdf_matches) == 1, "Found multiple post synthesis SDF files"
    assert len(verilog_matches) == 1, "Found multiple post synthesis Verilog files"


    return (blif_matches[0], verilog_matches[0], sdf_matches[0])


def create_modelsim_do(args, vcd_file, verilog_files, dut_inputs, dut_outputs, dut_clocks):
    do_lines = []

    do_lines.append("transcript on")
    do_lines.append("if {[file exists rtl_work]} {")
    do_lines.append("	vdel -lib rtl_work -all")
    do_lines.append("}")
    do_lines.append("vlib rtl_work")
    do_lines.append("vmap work rtl_work")
    do_lines.append("")
    for file in verilog_files:
        do_lines.append("vlog -sv -work work {" + file + "}")
    do_lines.append("")

    modelsim_cmd = "vsim -t 1ps -L rtl_work -L work -voptargs=\"+acc\" +transport_int_delays +transport_path_delays +sdf_verbose +bitblast tb"
    if args.modelsim_flavour == "modelsim_altera":
        #Include required altera libraries
        modelsim_cmd += " -L altera_mf_ver -L altera_ver -L lpm_ver -L sgate_ver -L stratixiv_hssi_ver -L stratixiv_pcie_hip_ver -L stratixiv_ver "
    do_lines.append(modelsim_cmd)
    do_lines.append("")
    do_lines.append("#Setup VCD logging")
    do_lines.append("vcd file {vcd_file}".format(vcd_file=vcd_file))
    for io in dut_inputs + dut_outputs + dut_clocks:
        do_lines.append("vcd add /tb/dut/{io}".format(io=io))
    do_lines.append("vcd add /{sim_clk}".format(sim_clk=SIM_CLOCK))
    do_lines.append("")
    do_lines.append("add wave /tb/*")
    do_lines.append("")
    do_lines.append("log /*")
    for io in dut_inputs + dut_outputs + dut_clocks:
        do_lines.append("log /tb/dut/{io}".format(io=io))
    do_lines.append("")
    do_lines.append("view structure")
    do_lines.append("view signals")
    do_lines.append("")
    do_lines.append("when {/tb/finished == \"1\"} {")
    do_lines.append("    vcd flush")
    do_lines.append("    simstats")
    do_lines.append("    stop")
    do_lines.append("    exit")
    do_lines.append("}")
    do_lines.append("")
    do_lines.append("run -all")

    return do_lines

def create_testbench(args, top_module, sdf_file, critical_path_delay_ps, dut_inputs, dut_outputs, dut_clocks):
    sim_clock_period = 2*args.cpd_scale*critical_path_delay_ps

    tb_lines = []
    tb_lines.append("`timescale 1ps/1ps")
    tb_lines.append("")
    tb_lines.append("module tb();")
    tb_lines.append("")
    tb_lines.append("//VPR Critical path delay: {cpd} ps".format(cpd=critical_path_delay_ps))
    tb_lines.append("localparam CLOCK_PERIOD = {period};".format(period=sim_clock_period))
    tb_lines.append("localparam CLOCK_DELAY = CLOCK_PERIOD / 2;")
    tb_lines.append("")
    tb_lines.append("//Total inputs width bits")
    tb_lines.append("localparam W = {W};".format(W=len(dut_inputs)))
    tb_lines.append("")
    tb_lines.append("//State width (used to iterate through all possible transitions)")
    tb_lines.append("localparam STATE_W = (2*W); ")
    tb_lines.append("")
    tb_lines.append("")
    tb_lines.append("//Testbench signals")
    tb_lines.append("logic [STATE_W-1:0] state;")
    tb_lines.append("logic [W-1:0] prev_state;")
    tb_lines.append("logic [W-1:0] next_state;")
    tb_lines.append("logic rolled_over;")
    tb_lines.append("logic finished;")
    tb_lines.append("")
    tb_lines.append("//Simulation clock")
    tb_lines.append("logic {sim_clk};".format(sim_clk=SIM_CLOCK))
    tb_lines.append("")

    if len(dut_clocks) > 0:
        tb_lines.append("//DUT clocks")
        for clock in dut_clocks:
            tb_lines.append("logic {sig};".format(sig=clock))

    tb_lines.append("")

    tb_lines.append("//DUT inputs")
    for input in dut_inputs:
        tb_lines.append("logic {sig};".format(sig=input))

    tb_lines.append("")

    tb_lines.append("//DUT outputs")
    for output in dut_outputs:
        tb_lines.append("logic {sig};".format(sig=output))

    tb_lines.append("")
    tb_lines.append("//Instantiate the dut")
    tb_lines.append("{top_module_type} dut ( .* );".format(top_module_type=top_module))
    tb_lines.append("")
    tb_lines.append("initial $sdf_annotate(\"{sdf_file}\", dut);".format(sdf_file=sdf_file))
    tb_lines.append("")
    tb_lines.append("initial {sim_clk} = '1;".format(sim_clk=SIM_CLOCK))
    tb_lines.append("always #CLOCK_DELAY {sim_clk} = ~{sim_clk};".format(sim_clk=SIM_CLOCK))
    tb_lines.append("")
    tb_lines.append("initial {rolled_over,state} = 0;")
    tb_lines.append("initial finished = 0;")
    tb_lines.append("")

    num_exhaustive_states = 4**len(dut_inputs)
    if args.sim_mode == "exhaustive":
        tb_lines.append("")
        tb_lines.append("//")
        tb_lines.append("//Exhaustive mode simulation")
        tb_lines.append("//     Exhaustive states: {num}".format(num=num_exhaustive_states))
        tb_lines.append("//")
        tb_lines.append("")
        tb_lines.append("assign prev_state = state[W-1:0];")
        tb_lines.append("assign next_state = state[STATE_W-1:W];")
        tb_lines.append("")
        tb_lines.append("//Set the inputs for this cycle")
        tb_lines.append("always @(posedge {sim_clk}) begin".format(sim_clk=SIM_CLOCK))
        tb_lines.append("    {" + ','.join(dut_inputs) + "} <= next_state[W-1:0];")
        tb_lines.append("")
        tb_lines.append("    //Finish after state repeats its first two values")
        tb_lines.append("    //This ensures that we cover the last transition for full coverage")
        tb_lines.append("    if(rolled_over && state == {{STATE_W-1{1'b0}},1'b1}) finished <= 1'b1;")
        tb_lines.append("")
        tb_lines.append("end")
        tb_lines.append("")
        tb_lines.append("//Set-up for the next clock edge")
        tb_lines.append("always @(negedge {sim_clk}) begin".format(sim_clk=SIM_CLOCK))
        tb_lines.append("    if(finished) $stop;")
        tb_lines.append("")
        tb_lines.append("    //Advance to next state")
        tb_lines.append("    {rolled_over,state} += 1;")
        tb_lines.append("")
        tb_lines.append("    {" + ','.join(dut_inputs) + "} <= prev_state[W-1:0];")
        tb_lines.append("end")
    else:
        assert args.sim_mode == "monte_carlo"

        #finish_count = int(math.ceil(args.monte_carlo_iter_fraction*num_exhaustive_states))

        tb_lines.append("")
        tb_lines.append("//")
        tb_lines.append("//Monte Carlo mode simulation")
        tb_lines.append("//     Exhaustive states: {num}".format(num=num_exhaustive_states))
        tb_lines.append("//     Monte Carlo Frac : {frac}".format(frac=args.monte_carlo_iter_fraction))
        #tb_lines.append("//     Finish count     : {cnt}".format(cnt=finish_count))
        tb_lines.append("//")
        tb_lines.append("")
        tb_lines.append("//State Counter: Exhaustive count {num}".format(num=num_exhaustive_states))
        tb_lines.append("logic [2**W-1:0] count;")
        tb_lines.append("initial count = 0;")
        tb_lines.append("")
        tb_lines.append("//Set the inputs for this cycle")
        tb_lines.append("always @(posedge {sim_clk}) begin".format(sim_clk=SIM_CLOCK))
        for input in dut_inputs:
            tb_lines.append("    {input} <= $urandom_range(1,0);".format(input=input))
        tb_lines.append("")
        tb_lines.append("    count <= count + 1;")
        tb_lines.append("")
        #tb_lines.append("    if(count >= {finish_count}) finished <= 1'b1;".format(finish_count=finish_count))
        tb_lines.append("end")

    tb_lines.append("")
    if len(dut_clocks) > 0:
        tb_lines.append("//Tie all dut clocks to the sim clock")
        for clock in dut_clocks:
            tb_lines.append("assign {dut_clk} = {sim_clk};".format(dut_clk=clock, sim_clk=SIM_CLOCK))
    tb_lines.append("")
    tb_lines.append("endmodule")

    return tb_lines

#
# Utility
#
def run_command(cmd, log_filename=None, verbose=False):
    #Convert all to string
    cmd = [str(x) for x in cmd]

    print "\t" + " " .join(cmd)
    sys.stdout.flush()

    cmd_runner = CommandRunner(verbose=verbose)

    if not log_filename:
        base_cmd = os.path.basename(cmd[0])
        log_filename = base_cmd + ".log"

    output, exitcode = cmd_runner.run_system_command(cmd, log_filename=log_filename, indent_depth=1)

    return output

def escape_name(name):
    return "\\" + name + " "

def unescape_name(name):
    return name.replace("\\", "")

if __name__ == "__main__":
    main()
