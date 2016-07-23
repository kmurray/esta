#!/bin/bash

for benchmark in $(cat /project/trees/vtr/vtr_flow/benchmarks/blif/circuits.txt)
do
    blif_file=/project/trees/vtr/vtr_flow/benchmarks/blif/6/$benchmark

    /project/trees/yosys/yosys -p "read_blif $blif_file; cd top; expose -evert-dff t:\$dff; delete t:\$dff; delete o:*.c %ci*; clean -purge; write_blif $benchmark;"
    abc -c "read_blif $benchmark; sweep; write_blif $benchmark"
done
