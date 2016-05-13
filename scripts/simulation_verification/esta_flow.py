#!/usr/bin/env python
import os
import sys
import argparse
import subprocess
import shutil
import re
import fnmatch

import pyverilog.vparser.parser as verilog_parser
import pyverilog.vparser.ast as vast
from pyverilog.ast_code_generator.codegen import ASTCodeGenerator

def parse_args():
    parser = argparse.ArgumentParser()

    #
    # General arguments
    #
    parser.add_argument("arch",
                        #required=True,
                        help="VTR FPGA Architecture description")

    parser.add_argument("blif",
                        #required=True,
                        help="Circuit BLIF file")

    parser.add_argument("--verify",
                        choices=["exhaustive", "sample"],
                        default="exhaustive",
                        help="Verification mode. Default: %(default)s")

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

    parser.add_argument("--compare",
                        action="store_true",
                        dest="run_compare",
                        default=False,
                        help="Run comparison between Simulation and ESTA for verification.")

    parser.add_argument("--outputs",
                        nargs="*",
                        default=None,
                        help="Outputs to compare")

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
    esta_arguments.add_argument("--esta_exec",
                                default="esta",
                                help="ESTA executable. Default %(default)s")

    #
    # Modelsim related arguments
    #
    modelsim_arguments = parser.add_argument_group("Modelsim", "Modelsim related options")
    modelsim_arguments.add_argument("--vsim_exec",
                                    default="vsim",
                                    help="Modelsim vsim executable. Default %(default)s")

    modelsim_arguments.add_argument("--fix_modelsim_altera_sdf_annotation",
                                    default=True,
                                    help="Fix bug with Modelsim-Altera where it will not allow SDF annotation without an instantiated Altera primitive")

    modelsim_arguments.add_argument("--link_verilog",
                                    nargs="+",
                                    default=['/project/trees/vtr/vtr_flow/primitives.v'],
                                    help="Extra verilog files to include in modelsim simulation (e.g. VTR's primitives.v)")

    modelsim_arguments.add_argument("--cpd_scale",
                                    default=1.3,
                                    type=float,
                                    help="Factor to expand critical path delay by to set modelsim clock period. Should be >= 1.0. Default: %(default)s")
    #
    # VCD Extraction arguments
    #
    transition_extraction_arguments = parser.add_argument_group("Transition Extraction", "Options for processing simulation VCD")
    transition_extraction_arguments.add_argument("--vcd_extract_exec",
                                                 default="vcd_extract",
                                                 help="Tool used to post-process VCD to extract transitions and delays. Default: %(default)s")

    #
    # Comparison/verification related arguments
    #
    transition_extraction_arguments = parser.add_argument_group("Comparision and Verification", "Options for comparing the ESTA and Simulation results")
    transition_extraction_arguments.add_argument("--comparison_exec",
                                                 default="compare_exhaustive.py",
                                                 help="Tool used compare ESTA and simulation results. Default: %(default)s")

    args = parser.parse_args()

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
    vpr_sdf_file = "top_post_synthesis.sdf"
    vpr_verilog_file = "top_post_synthesis.v"
    vpr_log = "vpr.log"

    if args.run_sim or args.run_esta:
        #Run VTR to generate an SDF file
        print
        print "Running VTR"
        vtr_results = run_vtr(args, vpr_log)

    vpr_cpd_ps = parse_vpr_cpd(vpr_log)

    #Extract port and top instance information
    print
    print "Extracting verilog information"
    design_info = extract_design_info(vpr_verilog_file, args.blif)

    #Run ESTA
    if args.run_esta:
        print
        print "Running ESTA"
        esta_results = run_esta(args, design_info=design_info, sdf_file=vpr_sdf_file)

    #Run Modelsim to collect simulation statistics
    if args.run_sim:

        print
        print "Running Modelsim"
        vcd_file = "sim.vcd"
        modelsim_results = run_modelsim(args, 
                                        sdf_file=vpr_sdf_file,
                                        cpd_ps=vpr_cpd_ps,
                                        verilog_info=design_info,
                                        vcd_file=vcd_file
                                        )

        print
        print "Extracting Transitions"
        transition_results = run_transition_extraction(args, vcd_file, design_info)

    if args.run_compare:
        print 
        print "Comparing ESTA and Simulation Results"
        run_comparison(args, design_info, vpr_cpd_ps)

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
            "-gen_postsynthesis_netlist", "on"
          ]

    output = run_command(cmd, log_filename=vpr_log_filename)

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
            "--dump_exhaustive_csv", ",".join(dump_outputs)
          ]
    run_command(cmd)

def run_modelsim(args, sdf_file, cpd_ps, verilog_info, vcd_file):
    top_verilog = verilog_info["file"]

    #Fix the input verilog for Modelsim-Altera if required
    if args.fix_modelsim_altera_sdf_annotation:
        modelsim_top_verilog = fix_modelsim_altera_sdf_annotation(top_verilog)
    else:
        modelsim_top_verilog = top_verilog


    #Write out the test bench
    tb_verilog = "tb.sv"
    with open(tb_verilog, "w") as f:
        for line in create_testbench(args, 
                                     sdf_file=sdf_file,
                                     critical_path_delay_ps=cpd_ps, 
                                     top_module=verilog_info['module'], 
                                     dut_inputs=verilog_info['inputs'], 
                                     dut_outputs=verilog_info['outputs']):
            print >>f, line

    #Write out the modelsim .do file
    sim_do_file = "tb.do"
    with open(sim_do_file, "w") as f:
        for line in create_modelsim_do(args, vcd_file, [modelsim_top_verilog, tb_verilog] + args.link_verilog):
            print >>f, line

    #Run the simulation
    cmd = [
            args.vsim_exec,
            "-c",
            "-do", sim_do_file
          ]

    run_command(cmd)

    return {
            'vcd_file': vcd_file,
           }

def run_transition_extraction(args, vcd_file, top_verilog_info):
    if args.outputs is None:
        outputs = top_verilog_info['outputs']
    else:
        outputs = args.outputs

    cmd = [
            args.vcd_extract_exec,
            vcd_file,
            '-c', 'clk',
          ]
    cmd += ["-i"] + top_verilog_info['inputs']
    cmd += ["-o"] + outputs

    run_command(cmd)

    return {}

def run_comparison(args, design_info, sta_cpd):
    if args.outputs is None:
        outputs = design_info['outputs']
    else:
        outputs = args.outputs

    for output in outputs:
        print "Comparing output: {port}".format(port=output)

        sim_csv = ".".join(["sim", output, "csv"])

        esta_csvs = []
        for filename in os.listdir(os.getcwd()):
            if fnmatch.fnmatch(filename, "esta." + output + "*.csv"):
                esta_csvs.append(filename)

        assert len(esta_csvs) > 0

        highest_node_num = -1
        for file in esta_csvs:
            esta, output, node, ext = file.split('.')

            node_id = int(node[1:])

            highest_node_num = max(highest_node_num, node_id)
        assert highest_node_num >= 0

        #Only analyze the last one (i.e. highest node number)
        esta_csv = "esta." + output + ".n" + str(highest_node_num) + ".csv"

        cmd = [
                args.comparison_exec,
                sim_csv,
                esta_csv,
                "--sta_cpd", sta_cpd,
                "--plot",
                "--plot_file", '.'.join([output, "histogram", "pdf"])
              ]

        run_command(cmd, log_filename='.'.join(["comparison", output, "log"]))

    return {}

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
    with open(blif_file) as f:
        for line in f:
            line.strip()
            if line.startswith(".inputs"):
                inputs = line.split()[1:]
                break
    assert inputs

    outputs = []
    for port in mod_def.portlist.ports:

        io = port.children()[0]

        if isinstance(io, vast.Output):
            outputs.append(io.name)
        else:
            assert isinstance(io, vast.Input)
            assert io.name in inputs

    return {
            'file': top_verilog,
            'module': name,
            'inputs': inputs,
            'outputs': outputs,
           }

def create_modelsim_do(args, vcd_file, verilog_files):
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
    do_lines.append("vsim -t 1ps -L rtl_work -L work -L altera_mf_ver -L altera_ver -L lpm_ver -L sgate_ver -L stratixiv_hssi_ver -L stratixiv_pcie_hip_ver -L stratixiv_ver -voptargs=\"+acc\" +sdf_verbose tb")
    do_lines.append("")
    do_lines.append("#Setup VCD logging")
    do_lines.append("vcd file {vcd_file}".format(vcd_file=vcd_file))
    do_lines.append("vcd add /tb/dut/*")
    do_lines.append("vcd add /clk")
    do_lines.append("")
    do_lines.append("add wave *")
    do_lines.append("")
    do_lines.append("log -r /*")
    do_lines.append("log -r /*/dut/*")
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

def create_testbench(args, top_module, sdf_file, critical_path_delay_ps, dut_inputs, dut_outputs):
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
    tb_lines.append("//Input width")
    tb_lines.append("localparam W = {W};".format(W=len(dut_inputs)))
    tb_lines.append("")
    tb_lines.append("//State width (used to iterate through all possible transitions)")
    tb_lines.append("localparam STATE_W = (2*W); ")
    tb_lines.append("")
    tb_lines.append("")
    tb_lines.append("//Testbench signals")
    tb_lines.append("logic [STATE_W-1:0] state;")
    tb_lines.append("logic rolled_over;")
    tb_lines.append("logic [W-1:0] prev_state;")
    tb_lines.append("logic [W-1:0] next_state;")
    tb_lines.append("logic finished;")
    tb_lines.append("")
    tb_lines.append("//Simulation clock")
    tb_lines.append("logic clk;")
    tb_lines.append("")
    tb_lines.append("//dut inputs")

    for input in dut_inputs:
        tb_lines.append("logic {sig};".format(sig=input))
        
    tb_lines.append("")

    tb_lines.append("//dut outputs")
    for output in dut_outputs:
        tb_lines.append("logic {sig};".format(sig=output))

    tb_lines.append("")
    tb_lines.append("//Instantiate the dut")
    tb_lines.append("{top_module_type} dut ( .* );".format(top_module_type=top_module))
    tb_lines.append("")
    tb_lines.append("initial $sdf_annotate(\"{sdf_file}\", dut);".format(sdf_file=sdf_file))
    tb_lines.append("")
    tb_lines.append("initial clk = '1;")
    tb_lines.append("always #CLOCK_DELAY clk = ~clk;")
    tb_lines.append("")
    tb_lines.append("initial {rolled_over,state} = 0;")
    tb_lines.append("initial finished = 0;")
    tb_lines.append("")
    tb_lines.append("assign prev_state = state[W-1:0];")
    tb_lines.append("assign next_state = state[STATE_W-1:W];")
    tb_lines.append("")
    tb_lines.append("//Set the inputs for this cycle")
    tb_lines.append("always @(posedge clk) begin")
    tb_lines.append("    {" + ','.join(dut_inputs) + "} <= next_state[W-1:0];")
    tb_lines.append("")
    tb_lines.append("    //Finish after state repeats its first two values")
    tb_lines.append("    //This ensures that we cover the last transition for full coverage")
    tb_lines.append("    if(rolled_over && state == {{STATE_W-1{1'b0}},1'b1}) finished <= 1'b1;")
    tb_lines.append("")
    tb_lines.append("end")
    tb_lines.append("")
    tb_lines.append("//Set-up for the next clock edge")
    tb_lines.append("always @(negedge clk) begin")
    tb_lines.append("    if(finished) $stop;")
    tb_lines.append("")
    tb_lines.append("    //Advance to next state")
    tb_lines.append("    {rolled_over,state} += 1;")
    tb_lines.append("")
    tb_lines.append("    {" + ','.join(dut_inputs) + "} <= prev_state[W-1:0];")
    tb_lines.append("end")
    tb_lines.append("")
    tb_lines.append("endmodule")

    return tb_lines

#
# Utility
#
def run_command(cmd, log_filename=None):
    #Convert all to string
    cmd = [str(x) for x in cmd]

    print "\t" + " " .join(cmd)
    sys.stdout.flush()

    output = None
    try:
        output = subprocess.check_output(" ".join(cmd), 
                                         shell=True,
                                         stderr=subprocess.STDOUT, #Also grab stderr
                                        )
    except subprocess.CalledProcessError as e:
        if e.returncode == 127:
            print "Error: Could not find executable '{executable}'".format(executable=cmd[0])
            sys.exit(1);
        raise e


    if not log_filename:
        base_cmd = os.path.basename(cmd[0])
        log_filename = base_cmd + ".log"

    with open(log_filename, "w") as f:
        print >>f, output

    return output



if __name__ == "__main__":
    main()
