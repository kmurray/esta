#include <cassert>
#include <unordered_map>
#include "BlifTimingGraphBuilder.hpp"

#include <iostream>
using std::cout;

#include "cuddObj.hh"
extern Cudd g_cudd;

namespace std {
    template <>
    struct hash<BDD> {
        size_t operator()(const BDD& x) const {
            return hash<void*>()(x.manager()) ^ hash<void*>()(x.getNode());
        }
    };
}

BlifTimingGraphBuilder::BlifTimingGraphBuilder(BlifData* data)
    : blif_data_(data) { }

void BlifTimingGraphBuilder::build(TimingGraph& tg) {
    /*
     * Note: a single primitive in the BLIF netlist
     * may become several nodes (one for each pin) 
     * in the timing graph.
     *
     * To build the timing graph we first convert
     * each primitive to its set of nodes (pins)
     * and fill in the edges internal to the 
     * primitive.
     *
     */
    assert(blif_data_->models.size() > 0);

    BlifModel* top_model = blif_data_->models[0];

    for(auto* input_port : top_model->inputs) {
        create_input(tg, input_port);
    }

    for(auto* blif_latch : top_model->latches) {
        create_latch(tg, blif_latch);
    }

    for(auto* blif_names : top_model->names) {
        create_names(tg, blif_names);
    }

    for(auto* blif_subckt : top_model->subckts) {
        create_subckt(tg, blif_subckt);
    }

    for(auto* output_port : top_model->outputs) {
        create_output(tg, output_port);
    }

    /*
     * Once we have processed every primtiive, we then
     * walk through all the nets (external to primitives)
     * in the netlist and add edges to represent them.
     */
    create_net_edges(tg);
}

void BlifTimingGraphBuilder::create_input(TimingGraph& tg, const BlifPort* input_port) {
    //An input becomes the following in the timing graph:
    //    INPAD_SOURCE ---> INPAD_OPIN


    //Creat the variable representing this input.
    //  Note that this is used for both the source and opin since they are logically equivalent
    BDD var = g_cudd.bddVar();

    NodeId src = tg.add_node(TN_Type::INPAD_SOURCE, 0, false, var); //Default to 1st clock domain
    NodeId opin = tg.add_node(TN_Type::INPAD_OPIN, INVALID_CLOCK_DOMAIN, false, var);

    //Add the edge between them
    tg.add_edge(src, opin);

    //Record the mapping from blif netlist to timing graph nodes
    assert(port_to_node_lookup.find(input_port) == port_to_node_lookup.end()); //No found
    port_to_node_lookup[input_port] = opin;
}

void BlifTimingGraphBuilder::create_output(TimingGraph& tg, const BlifPort* output_port) {
    //An output becomes the following in the timing graph:
    //    OUTPAD_IPIN ---> OUTPAD_SINK
    
    //Creat the variable representing this input.
    //  Note that this is used for both the source and opin since they are logically equivalent
    BDD var = g_cudd.bddVar();

    NodeId ipin = tg.add_node(TN_Type::OUTPAD_IPIN, INVALID_CLOCK_DOMAIN, false, var);
    NodeId sink = tg.add_node(TN_Type::OUTPAD_SINK, INVALID_CLOCK_DOMAIN, false, var);

    //Add the edge between them
    tg.add_edge(ipin, sink);

    //Record the mapping from blif netlist to timing graph nodes
    assert(port_to_node_lookup.find(output_port) == port_to_node_lookup.end()); //No found
    port_to_node_lookup[output_port] = ipin;
}

void BlifTimingGraphBuilder::create_latch(TimingGraph& tg, const BlifLatch* latch) {
    //A latch becomes multiple nodes as follows:
    //
    //   FF_IPIN ---> FF_SINK             FF_SOURCE ---> FF_OPIN
    //     (d)           ^                    ^            (q)
    //                   |                    |
    //                   |                    |
    //   FF_CLOCK -----------------------------
    //     (clk)

    assert(latch->type == LatchType::RISING_EDGE);

    //Creat the variable representing this input.
    BDD var_d = g_cudd.bddVar();
    BDD var_q = g_cudd.bddVar();
    BDD var_clk = g_cudd.bddVar();

    //The nodes
    NodeId ipin = tg.add_node(TN_Type::FF_IPIN, INVALID_CLOCK_DOMAIN, false, var_d);
    NodeId sink = tg.add_node(TN_Type::FF_SINK, INVALID_CLOCK_DOMAIN, false, var_d);
    NodeId src  = tg.add_node(TN_Type::FF_SOURCE, INVALID_CLOCK_DOMAIN, false, var_q);
    NodeId opin = tg.add_node(TN_Type::FF_OPIN, INVALID_CLOCK_DOMAIN, false, var_q);
    NodeId clock = tg.add_node(TN_Type::FF_CLOCK, INVALID_CLOCK_DOMAIN, false, var_clk); 

    //The edges
    tg.add_edge(ipin, sink);
    tg.add_edge(src, opin);
    tg.add_edge(clock, sink);
    tg.add_edge(clock, src);

    //Record the mapping from blif netlist to timing graph nodes
    port_to_node_lookup[latch->input] = ipin;
    port_to_node_lookup[latch->output] = opin;
    port_to_node_lookup[latch->control] = clock;
}

void BlifTimingGraphBuilder::create_names(TimingGraph& tg, const BlifNames* names) {
    /*
     * A blif .names structure represents a multiple-input single-output
     * logic function.
     * 
     * We create a PRIMITIVE_IPIN for each input, and a PRIMTITIVE_OPIN for the output.
     * Edges connect each IPIN to the OPIN:
     *    
     *       PRIMTIVE_IPIN --------
     *                             \
     *       PRIMTIVE_IPIN ---------PRIMITIVE_OPIN
     *                             /
     *       PRIMTIVE_IPIN --------
     */
     
    //Build the nodes
    std::vector<NodeId> input_ids;
    std::vector<BDD> input_vars;
    for(size_t i = 0; i < names->ports.size() - 1; i++) {
        const BlifPort* input_port = names->ports[i];

        BDD var = g_cudd.bddVar();
        NodeId node_id = tg.add_node(TN_Type::PRIMITIVE_IPIN, INVALID_CLOCK_DOMAIN, false, var);
        input_ids.push_back(node_id);
        input_vars.push_back(var);

        //Record the mapping from blif netlist to timing graph nodes
        port_to_node_lookup[input_port] = node_id;
    }

    BDD f = create_func_from_names(names, input_vars);

    const BlifPort* output_port = names->ports[names->ports.size()-1];
    NodeId output_node_id = tg.add_node(TN_Type::PRIMITIVE_OPIN, INVALID_CLOCK_DOMAIN, false, f);

    //Record the mapping from blif netlist to timing graph nodes
    port_to_node_lookup[output_port] = output_node_id;

    //Build the internal edges
    for(NodeId input_node_id : input_ids) {
        tg.add_edge(input_node_id, output_node_id);
    }
}

void BlifTimingGraphBuilder::create_subckt(TimingGraph& tg, const BlifSubckt* names) {
    assert(0); //Unimplemented
    //TODO: determine whether a port is combinational or clocked
}

void BlifTimingGraphBuilder::create_net_edges(TimingGraph& tg) {
    /*
     * Walk through each net connecting drivers to sinks.
     *
     * We also collect equivalent variables, which we tell
     * the BDD package to re-map.
     */

    for(const BlifNet* net : blif_data_->nets) {
        cout << "Net: " << *net->name << "\n";
        assert(net->drivers.size() == 1);
        const BlifPort* driver_port= net->drivers[0]->port;
        
        auto iter_driver = port_to_node_lookup.find(driver_port);
        assert(iter_driver != port_to_node_lookup.end());

        NodeId driver_node = iter_driver->second;

        for(const BlifPortConn* sink_conn : net->sinks) {
            const BlifPort* sink_port = sink_conn->port;

            auto iter_sink = port_to_node_lookup.find(sink_port);
            assert(iter_sink != port_to_node_lookup.end());

            NodeId sink_node = iter_sink->second;        

            tg.add_edge(driver_node, sink_node);
        }
    }

    //Now that we have all the edges in the graph we can levelize it
    tg.levelize();


    //Print the updated logic
    cout << "Orig Logic:" << "\n";
    for(NodeId id = 0; id < tg.num_nodes(); id++) {
        cout << "Node " << id << ": " << tg.node_func(id) << "\n";
    }

    //Now we need to walk through the graph and identify which BDD
    //vars are redundant (i.e. equivalent with others due to nets -- which are identity logic functions)
    //
    //We will collect these variables and swap them with the fundamental variables
    //
    //Note that since these variables may be cascaded several levels deep we need to
    //investigate swaps in topological order, which is why we do this after levelizing the graph

    std::unordered_map<BDD,BDD> vars_to_replace;

    for(int i = 0; i < tg.num_levels() - 1; i++) {
        //Vars to replace at this level

        //Identify the variables
        for(NodeId driver_node_id : tg.level(i)) {
            auto driver_var = tg.node_func(driver_node_id);

            for(int edge_idx = 0; edge_idx < tg.num_node_out_edges(driver_node_id); edge_idx++) {
                EdgeId edge_id = tg.node_out_edge(driver_node_id, edge_idx);
                TE_Type edge_type = tg.edge_type(edge_id);

                if(edge_type == TE_Type::NET) {
                    //This is an net connection (i.e. identity logic function)
                    //so we can replace the downstream variable with the upstream

                    NodeId sink_node_id = tg.edge_sink_node(edge_id);
                    auto sink_var = tg.node_func(sink_node_id);

                    //Replace sink_var
                    //
                    //We first check if the driver var has already been replaced
                    //if so we use it as the replacement.
                    //
                    //Otherwise we directly replace with driver var
                    auto iter = vars_to_replace.find(driver_var);
                    if(iter != vars_to_replace.end()) {
                        vars_to_replace[sink_var] = iter->second;
                    } else {
                        vars_to_replace[sink_var] = driver_var;
                    }
                }
            }
        }

        cout << "Vars to replace: \n";
        for(auto kv : vars_to_replace) {
            cout << "\t" << kv.first << " -> " << kv.second << "\n";
        }


        //Do the replacement
        for(NodeId node_id : tg.level(i+1)) {
            std::vector<BDD> curr_vars;
            std::vector<BDD> new_vars;

            BDD& f = tg.node_func(node_id);
            cout << "F: " << f << "\n";

            //BDD support_var;
            DdGen* gen;
            DdNode* node;
            Cudd_ForeachNode(f.manager(), f.getNode(), gen, node) {
                 BDD support_var = BDD(g_cudd, node);

                 cout << "\tF var: " << support_var << "\n";
                 auto iter = vars_to_replace.find(support_var);
                 if(iter != vars_to_replace.end()) {
                     //Found it, this var should be replaced!
                     BDD new_var = iter->second;
 
                     cout << "\tSwapping '" << support_var << "' for '" << new_var << "'\n";
 
                     curr_vars.push_back(support_var);
                     new_vars.push_back(new_var);
                 }
            }
/*
 *            for(DdGen* gen = f.FirstNode(&support_var); g_cudd.NextNode(gen, &support_var); ) {
 *                cout << "\tF var: " << support_var << "\n";
 *                auto iter = vars_to_replace.find(support_var);
 *                if(iter != vars_to_replace.end()) {
 *                    //Found it, this var should be replaced!
 *                    BDD new_var = iter->second;
 *
 *                    cout << "\tSwapping '" << support_var << "' for '" << new_var << "'\n";
 *
 *                    curr_vars.push_back(support_var);
 *                    new_vars.push_back(new_var);
 *                }
 *            }
 */

            BDD new_f = f.SwapVariables(curr_vars, new_vars);

            //Update original
            f = new_f;
        }



    }

/*
 *
 *    //Do the replacement
 *    for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
 *        std::vector<BDD> curr_vars;
 *        std::vector<BDD> new_vars;
 *
 *        BDD& f = tg.node_func(node_id);
 *
 *        for(auto support_var_idx : f.SupportIndices()) {
 *            auto iter = vars_to_replace.find(g_cudd.bddVar(support_var_idx));
 *            if(iter != vars_to_replace.end()) {
 *                //Found it, this var should be replaced!
 *                BDD support_var = g_cudd.bddVar(support_var_idx);
 *                BDD new_var = g_cudd.bddVar(vars_to_replace[support_var]);
 *
 *                curr_vars.push_back(support_var);
 *                new_vars.push_back(new_var);
 *            }
 *        }
 *
 *        BDD new_f = f.SwapVariables(curr_vars, new_vars);
 *
 *        //Update original
 *        f = new_f;
 *    }
 */

    cout << "Updated Logic:" << "\n";
    for(NodeId id = 0; id < tg.num_nodes(); id++) {
        cout << "Node " << id << ": " << tg.node_func(id) << "\n";
    }

    /*
     *cout << "Vars to replace: \n";
     *for(auto kv : vars_to_replace) {
     *    cout << "\t" << kv.first << " -> " << kv.second << "\n";
     *}
     */

}

BDD BlifTimingGraphBuilder::create_func_from_names(const BlifNames* names, std::vector<BDD> input_vars) {
    //Convert the single-output cover representing the .names logic function
    //into a BDD function of the input variables.
    //
    //The .names defines the logic function as a single-output cover
    //e.g.
    //
    //.name a b c out
    //000 1
    //101 1
    //0-1 1
    //
    //Each row represents an input cube and the associated output logic value.
    //That is, the first part of the row represents the values of the inputs and
    //the last the value of the input.
    //
    //For example, the above represents the following logic function:
    //
    // out = (!a . !b . !c) + (a . !b . c) + (!a . c)
    //
    //Where '.' represents logic and, and '+' represents logical or.
    //Each expression in parenthesies corresponds to a row in the single
    //output cover

    //We initially make the function false, since we are adding the
    //ON set from the .names
    BDD f = g_cudd.bddZero();

    for(std::vector<LogicValue>* row : names->cover_rows) {
        //We expect only the on-set to be defined, so
        //the last element in a row (the output truth value)
        //must be TRUE
        assert((*row)[row->size()-1] == LogicValue::TRUE);

        //We now and together the inputs in a single row
        //to create a cube
        BDD cube = g_cudd.bddOne();
        for(size_t i = 0; i < row->size() - 1; i++) {
            LogicValue val = (*row)[i];
            BDD var = input_vars[i];

            if(val == LogicValue::TRUE) {
                cube &= var;
            } else if (val == LogicValue::FALSE) {
                cube &= !var;
            } else if (val == LogicValue::DC) {
                //DC values are ignored (they don't appear in the cube)
                continue;
            } else {
                assert(0);
            }
        }

        //Add the row to the function
        f |= cube;
    }

    return f;
}
