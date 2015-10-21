#!/usr/bin/env bash

reorder_methods=(
    CUDD_REORDER_SIFT 
    CUDD_REORDER_SIFT_CONVERGE 
    CUDD_REORDER_SYMM_SIFT_CONV 
    CUDD_REORDER_GROUP_SIFT
    CUDD_REORDER_GROUP_SIFT_CONV
    CUDD_REORDER_WINDOW2
#    CUDD_REORDER_WINDOW3
    CUDD_REORDER_WINDOW4
    CUDD_REORDER_WINDOW2_CONV
#    CUDD_REORDER_WINDOW3_CONV
    CUDD_REORDER_WINDOW4_CONV
    )
benchmarks=(
    ex5p 
    misex3 
    apex4 
    alu4 
    seq 
    apex2 
#    des 
#    spla 
#    pdc 
    ex1010
    )
benchmark_dir=~/trees/vtr/vtr_flow/benchmarks/blif

for method in "${reorder_methods[@]}"
do
    for benchmark in "${benchmarks[@]}"
    do
        echo -n "echo 'Starting $benchmark $method'"
        echo -n " && "
        echo -n "/usr/bin/time -v /home/kmurray/trees/tatum_bdd/build_release/src/eta -b $benchmark_dir/$benchmark.blif --reorder_method $method >& ${method}_${benchmark}.log"
        echo -n " && "
        echo -n "echo 'Ending   $benchmark $method'"
        echo ""
    done
done
