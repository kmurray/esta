#!/usr/bin/env python3

import sys
from ortools.linear_solver import pywraplp
from collections import OrderedDict

from pyeda.boolalg.minimization import espresso_exprs
from pyeda.boolalg.table import truthtable, truthtable2expr
from pyeda.boolalg.bfarray import exprvars

#TRANSITIONS = ['r', 'f', 'h', 'l']
#INPUTS = ['A', 'B']

#def main():
    #N = 4
    #num_minterms = 2**N
    
    #prob = {}

    #for t_a in TRANSITIONS:
        #for t_b in TRANSITIONS:

            #f_a = 'A' + t_a
            #f_b = 'B' + t_b

            #p = None
            #if t_a == 'r' and t_b == 'r':
                #p = 2. / 16
            #elif t_a == 'f' and t_b == 'r':
                #p = 0. / 16
            #else:
                #p = 1. / 16

            #assert p != None

            #prob[(('A', t_a), ('B', t_b))] = p

TRANSITIONS = ['r', 'f', 'h', 'l']
INPUTS = ['A', 'B']#, 'C']

def main():
    N = 4
    num_minterms = 2**N

    X = exprvars('x', N) #Boolean vars
    
    prob = {}

    for t_a in TRANSITIONS:
        for t_b in TRANSITIONS:

            f_a = 'A' + t_a
            f_b = 'B' + t_b

            p = None
            if t_a == 'r':
                p = 1. / 16
            elif t_a == 'f' and (t_b == 'h' or t_b == 'l'):
                p = 1. / 8
            elif t_a == 'h':
                if t_b == 'f':
                    p = 1. / 8
                elif  t_b == 'h':
                    p = 1. / 4
                else:
                    p = 0.
            elif t_a == 'l' and t_b == 'r':
                p = 1. / 8
            else:
                p = 0. / 16

            assert p != None

            prob[(('A', t_a), ('B', t_b))] = p

    #for t_a in TRANSITIONS:
        #for t_c in TRANSITIONS:

            #f_a = 'A' + t_a
            #f_c = 'C' + t_c

            #p = None
            #if t_a in ['r', 'f'] and t_c in ['r', 'f']:
                #p = 4. / 16
            #else:
                #p = 0. / 16
            #assert p != None

            #prob[(('A', t_a), ('C', t_c))] = p

    #for t_b in TRANSITIONS:
        #for t_c in TRANSITIONS:
            #f_b = 'B' + t_b
            #f_c = 'C' + t_c

            #p = None
            #if (t_b == 'r' and t_c =='r') or (t_b == 'f' and t_c == 'f') or (t_b == 'h' and t_c == 'r') or (t_b == 'l' and t_c == 'f'):
                #p = 4. / 16
            #else:
                #p = 0. / 16
            #assert p != None

            #prob[(('B', t_b), ('C', t_c))] = p


    #Verify that all partial sums are valid




    minterm_counts = dict([(key, int(p*num_minterms)) for key, p in prob.items()])

    print("Minterm counts:")
    for key, value in minterm_counts.items():
        print(key, value)

    #assert sum(minterm_counts.values()) == len(INPUTS)*num_minterms

    solver = pywraplp.Solver('SolveIntegerPorblem',
                            pywraplp.Solver.CBC_MIXED_INTEGER_PROGRAMMING)

    print("Generating Vars")
    #Create the minterm variables
    minterm_vars = OrderedDict()
    for minterm in range(0, num_minterms):
        for input in INPUTS:
            for trans in TRANSITIONS:
                
                var_name = "{}_{}_{}".format(minterm, input, trans)
                minterm_vars[(minterm, input, trans)] = solver.BoolVar(var_name)

    #Create the pair-wise boolean minterm variables
    minterm_pair_vars = OrderedDict()
    for i in range(len(INPUTS)):
        for j in range(i+1, len(INPUTS)):
            assert i != j

            input1 = INPUTS[i]
            input2 = INPUTS[j]
            
            for trans1 in TRANSITIONS:
                for trans2 in TRANSITIONS:
                    for minterm in range(0, num_minterms):

                        minterm_var1_key = (minterm, input1, trans1)
                        minterm_var2_key = (minterm, input2, trans2)

                        var_name = "{} & {}".format(str(minterm_vars[minterm_var1_key]), str(minterm_vars[minterm_var2_key]))

                        minterm_pair_vars[(minterm_var1_key, minterm_var2_key)] = solver.BoolVar(var_name)

    #print "Minterm vars:"
    #for key, value in minterm_vars.iteritems():
        #print key, value

    #print "Minterm pair-wise AND vars:"
    #for key, value in minterm_pair_vars.iteritems():
        #print key, value

    print("Generating Constraints")

    #Generate the pair-wise boolean AND constraints
    # to ensure the minterm pair variables define logical AND
    for (minterm_var1_key, minterm_var2_key), and_var in minterm_pair_vars.items():
        minterm_var1 = minterm_vars[minterm_var1_key]
        minterm_var2 = minterm_vars[minterm_var2_key]

        # x_{ikt} + x_{ijt} = y_{ikjt} <= 1
        constr1 = solver.Constraint(-solver.infinity(), 1)
        constr1.SetCoefficient(minterm_var1, 1)
        constr1.SetCoefficient(minterm_var2, 1)
        constr1.SetCoefficient(and_var, -1)

        # y_{ikjt} - x_{ikt} <= 0
        constr2 = solver.Constraint(-solver.infinity(), 0)
        constr2.SetCoefficient(and_var, 1)
        constr2.SetCoefficient(minterm_var1, -1)

        # y_{ikjt} - x_{ijt} <= 0
        constr2 = solver.Constraint(-solver.infinity(), 0)
        constr2.SetCoefficient(and_var, 1)
        constr2.SetCoefficient(minterm_var2, -1)

    #Generate the single minterm per primary-input constraints
    for minterm in range(0, num_minterms):
        for input in INPUTS:

            #  -----
            #  \    
            #   |    x_{ijt} = 1, for all minterms, for all input
            #  /    
            #  -----
            #  t in trans
            constr = solver.Constraint(1,1)
            for trans in TRANSITIONS:
                minterm_var = minterm_vars[(minterm, input, trans)]
                constr.SetCoefficient(minterm_var, 1)


    #Generate the pair-wise count constraints
    for pair, count in minterm_counts.items():

        first, second = pair

        first_input, first_trans = first
        second_input, second_trans = second

        assert first_input != second_input

        #  -----           -----
        #  \               \
        #   |               |           y_{ikjt} = #(f_{kt} & f_{jt}), for all unique input pairs
        #  /               /
        #  -----           -----
        #  i in minterms   t in trans
        constr = solver.Constraint(count, count)
        for minterm in range(0, num_minterms):
            first_minterm_key = (minterm, first_input, first_trans)
            second_minterm_key = (minterm, second_input, second_trans)

            and_var = minterm_pair_vars[(first_minterm_key,second_minterm_key)]

            constr.SetCoefficient(and_var, 1)

    print("Starting Solve")
    status = solver.Solve()

    print("Solver took {:.2f} sec".format(solver.WallTime() / 1000))
    if status == solver.OPTIMAL:
        print("Solved optimal (OPTIMAL)")
    elif status == solver.FEASIBLE:
        print("Solved non-optimal (FEASIBLE)")
    elif status == solver.INFEASIBLE:
        print("Failed to solve (INFEASIBLE)")
        sys.exit(1)
    else:
        print("Failed to solve (status unkown)")
        sys.exit(2)


    # The solution looks legit (when using solvers other than
    # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
    assert solver.VerifySolution(1e-7, True)

    print('Number of variables =', solver.NumVariables()) 
    print('Number of constraints =', solver.NumConstraints()) 

    # The objective value of the solution.
    print('Optimal objective value = %d' % solver.Objective().Value()) 
    print()

    # The value of each variable in the solution.
    #variable_list = [var for var in minterm_vars.values()] + [var for var in minterm_pair_vars.values()]
    #for variable in variable_list:
        #print '%s = %d' % (variable.name(), variable.solution_value()) 

    cond_funcs = {}

    for input in INPUTS:
        for trans in TRANSITIONS:
            cond_funcs[(input, trans)] = []

    for key, var in minterm_vars.items():
        if var.solution_value() == 1:
            minterm, input, trans = key

            cond_func_key = (input, trans)

            cond_funcs[cond_func_key].append(minterm)

    for cond_func_key, minterms in cond_funcs.items():
        print("{} = {} = {}".format(cond_func_key, minterms, minterms_to_func(X, minterms)))

    verify_condition_functions(cond_funcs, minterm_counts, N, I=len(INPUTS), X=X)

def espresso_expr_safe(expr):
    expr_dnf = expr.to_dnf()
    if not (expr_dnf.is_one() or expr_dnf.is_zero()):
        return espresso_exprs(expr_dnf)[0]
    else:
        return expr_dnf

def minterms_to_func(X, minterms):

    tt_output_vec = [0 for x in range(2**len(X))]

    for minterm in minterms:
        tt_output_vec[minterm] = 1

    tt_output_str = ''.join([str(v) for v in tt_output_vec])

    f_tt = truthtable(X, tt_output_str)
    f_expr = truthtable2expr(f_tt)

    return espresso_expr_safe(f_expr)


def verify_condition_functions(cond_funcs, minterm_counts, N, I, X):
    #assert sum(minterm_counts.values()) == (I-1) * 2**N, "Pair-wise minterm counts must total to total number of minterms"

    #Verify the pair-wise constraints
    for (first_key, second_key), minterm_count in minterm_counts.items():

        common_minterms = set(cond_funcs[first_key]) & set(cond_funcs[second_key])

        assert len(common_minterms) == minterm_count, "Pair wise constraints must be satisfied for {} {}".format(first_key, second_key)

        print("{} & {}= {} / {} = {} = {} ".format(first_key, second_key, minterm_count, 2**N, float(minterm_count) / 2**N, minterms_to_func(X, common_minterms)))


    #Determine the (unique) minterms for each input
    input_minterms = {}
    for (input, trans), minterms in cond_funcs.items():
        if input not in input_minterms:
            input_minterms[input] = set()

        input_minterms[input] |= set(minterms)

    #Verify that the total number of minterms per input is 2**N
    for input, minterms in input_minterms.items():

        assert len(minterms) == 2**N, "Each PI must have 2**N minterms assigned to it"

if __name__ == "__main__":
    main()
