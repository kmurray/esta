#!/usr/bin/env python
import os
import sys
import argparse
import subprocess
import shutil
import re

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
                        help="")

    parser.add_argument("--sim",
                        action="store_true",
                        dest="run_sim",
                        default=False,
                        help="")

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
                                default="eta",
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
    args = parser.parse_args()

    return args

def main():
    args = parse_args()

    #Run VTR to generate an SDF file
    print "Running VTR"
    vtr_results = run_vtr(args)

    #Run ESTA
    if args.run_esta:
        print "Running ESTA"
        esta_results = run_esta(args, vtr_results['post_synth_sdf'])

    #Run Modelsim to collect simulation statistics
    if args.run_sim:
        print "Running Modelsim"
        modelsim_results = run_modelsim(args, 
                                        sdf_file=vtr_results['post_synth_sdf'],
                                        cpd_ps=vtr_results['critical_path_delay_ps'],
                                        top_verilog=vtr_results['post_synth_verilog'],
                                        vcd_file="dump.vcd"
                                        )

def run_vtr(args):

    #Localize the blif file, since VPR likes to put its files next to it
    orig_blif_location = args.blif
    new_blif_location = os.path.join(os.getcwd(), os.path.basename(orig_blif_location))
    shutil.copy(orig_blif_location, new_blif_location)

    args.blif = new_blif_location
    
    cmd = [
            args.vpr_exec,
            args.arch,
            args.blif,
            "-gen_postsynthesis_netlist", "on"
          ]

    output = run_command(cmd)

    cpd_ns_regex = re.compile(r".*Final critical path: (?P<cpd_ns>[\d.]+) ns", re.DOTALL)

    match = cpd_ns_regex.match(output)

    assert match, "Could not find VPR's critical path delay"

    cpd_ns = float(match.group("cpd_ns"))

    return {
            'post_synth_verilog': 'top_post_synthesis.v',
            'post_synth_sdf': 'top_post_synthesis.sdf',
            'critical_path_delay_ps': cpd_ns*1000,
           }

def run_esta(args, sdf_file):
    cmd = [
            args.esta_exec,
            "-b", args.blif,
            "-s", sdf_file

          ]
    run_command(cmd)

def run_modelsim(args, sdf_file, cpd_ps, top_verilog, vcd_file):
    #Extract port and top instance information
    top_verilog_info = extract_top_verilog_info(top_verilog)

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
                                     top_module=top_verilog_info['module'], 
                                     dut_inputs=top_verilog_info['inputs'], 
                                     dut_outputs=top_verilog_info['outputs']):
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

def extract_top_verilog_info(top_verilog):
    #
    #Load the original verilog
    #
    ast, directives = verilog_parser.parse([top_verilog])

    #
    #Add an unused stratixiv cell
    #
    mod_def = ast.description.definitions[0] #Definition of top

    name = mod_def.name

    inputs = []
    outputs = []
    for port in mod_def.portlist.ports:

        io = port.children()[0]

        if isinstance(io, vast.Output):
            outputs.append(io.name)
        else:
            assert isinstance(io, vast.Input)
            inputs.append(io.name)

    return {
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
    do_lines.append("vsim -t 1ps -L rtl_work -L work -L altera_mf_ver -L altera_ver -L lpm_ver -L sgate_ver -L stratixiv_hssi_ver -L stratixiv_pcie_hip_ver -L stratixiv_ver -voptargs=\"+acc\"  tb")
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
def run_command(cmd):
    print "\t" + " " .join(cmd)
    sys.stdout.flush()

    output = subprocess.check_output(" ".join(cmd), 
                                     shell=True,
                                     stderr=subprocess.STDOUT, #Also grab stderr
                                    )

    base_cmd = os.path.basename(cmd[0])

    with open(base_cmd + ".log", "w") as f:
        print >>f, output

    return output



if __name__ == "__main__":
    main()
