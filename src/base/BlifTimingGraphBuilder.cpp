#include <cassert>
#include <unordered_map>
#include "BlifTimingGraphBuilder.hpp"

#include <iostream>
using std::cout;

#include "bdd.hpp"

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


    /*
     *annotate_logic_functions(tg);
     */
}

void BlifTimingGraphBuilder::create_input(TimingGraph& tg, const BlifPort* input_port) {
    //An input becomes the following in the timing graph:
    //    INPAD_SOURCE ---> INPAD_OPIN


    NodeId src = tg.add_node(TN_Type::INPAD_SOURCE, 0, false); //Default to 1st clock domain
    NodeId opin = tg.add_node(TN_Type::INPAD_OPIN, INVALID_CLOCK_DOMAIN, false);

    //Add the edge between them
    tg.add_edge(src, opin);

    //Identity logic function
    tg.set_node_func(src, g_cudd.bddVar(0));
    tg.set_node_func(opin, g_cudd.bddVar(0));

    //Record the mapping from blif netlist to timing graph nodes
    assert(port_to_node_lookup.find(input_port) == port_to_node_lookup.end()); //No found
    port_to_node_lookup[input_port] = opin;
}

void BlifTimingGraphBuilder::create_output(TimingGraph& tg, const BlifPort* output_port) {
    //An output becomes the following in the timing graph:
    //    OUTPAD_IPIN ---> OUTPAD_SINK
    
    NodeId ipin = tg.add_node(TN_Type::OUTPAD_IPIN, INVALID_CLOCK_DOMAIN, false);
    NodeId sink = tg.add_node(TN_Type::OUTPAD_SINK, INVALID_CLOCK_DOMAIN, false);

    //Add the edge between them
    tg.add_edge(ipin, sink);

    //Identity logic function
    tg.set_node_func(ipin, g_cudd.bddVar(0));
    tg.set_node_func(sink, g_cudd.bddVar(0));

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

    //The nodes
    NodeId ipin = tg.add_node(TN_Type::FF_IPIN, INVALID_CLOCK_DOMAIN, false);
    NodeId sink = tg.add_node(TN_Type::FF_SINK, INVALID_CLOCK_DOMAIN, false);
    NodeId src  = tg.add_node(TN_Type::FF_SOURCE, INVALID_CLOCK_DOMAIN, false);
    NodeId opin = tg.add_node(TN_Type::FF_OPIN, INVALID_CLOCK_DOMAIN, false);
    NodeId clock = tg.add_node(TN_Type::FF_CLOCK, INVALID_CLOCK_DOMAIN, false); 

    //The edges
    tg.add_edge(ipin, sink);
    tg.add_edge(src, opin);
    tg.add_edge(clock, sink);
    tg.add_edge(clock, src);


    //Identity logic function from ipin -> sink
    tg.set_node_func(sink, g_cudd.bddVar(0));

    //Identity logic function from source -> opin
    tg.set_node_func(opin, g_cudd.bddVar(0));

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

        NodeId node_id = tg.add_node(TN_Type::PRIMITIVE_IPIN, INVALID_CLOCK_DOMAIN, false);
        input_ids.push_back(node_id);

        input_vars.push_back(g_cudd.bddVar(i));

        //Record the mapping from blif netlist to timing graph nodes
        port_to_node_lookup[input_port] = node_id;
    }

    const BlifPort* output_port = names->ports[names->ports.size()-1];
    NodeId output_node_id = tg.add_node(TN_Type::PRIMITIVE_OPIN, INVALID_CLOCK_DOMAIN, false);

    //Record the mapping from blif netlist to timing graph nodes
    port_to_node_lookup[output_port] = output_node_id;

    //Define the opin logic function in terms of its input edges
    BDD opin_node_func = create_func_from_names(names, input_vars);
    tg.set_node_func(output_node_id, opin_node_func);

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

            //Identity logic function
            tg.set_node_func(sink_node, g_cudd.bddVar(0));

            tg.add_edge(driver_node, sink_node);
        }
    }

    //Now that we have all the edges in the graph we can levelize it
    tg.levelize();


}

/*
 *void BlifTimingGraphBuilder::annotate_logic_functions(TimingGraph& tg) {
 *    //We walk through the graph in a levelized manner propogating logic functions
 *    //TODO: parallelize? --> need to be careful with cudd
 *    
 *    //Build the inverse lookup form node_id to BlifPort*
 *    for(auto& kv : port_to_node_lookup) {
 *        node_to_port_lookup[kv.second] = kv.first;
 *    }
 *    
 *    //In the levelized graph the primary inputs are the actual variables
 *    //so initialize them first
 *    for(NodeId pi_id : tg.primary_inputs()) {
 *        tg.set_node_func(pi_id, g_cudd.bddVar());
 *    }
 *
 *    for(int i = 1; i < tg.num_levels(); i++) {
 *        for(NodeId node_id : tg.level(i)) {
 *            TN_Type node_type = tg.node_type(node_id);
 *
 *            switch(node_type) {
 *                case TN_Type::INPAD_OPIN: {         //Fallthrough
 *                } case TN_Type::OUTPAD_SINK: {      //Fallthrough
 *                } case TN_Type::FF_SINK: {          //Fallthrough
 *                } case TN_Type::PRIMITIVE_IPIN: {   //Fallthrough
 *                } case TN_Type::OUTPAD_IPIN: {      //Fallthrough
 *                } case TN_Type::FF_OPIN: {     
 *                    //Single input node with identity logic function
 *                    assert(tg.num_node_in_edges(node_id) == 1);
 *
 *                    EdgeId in_edge = tg.node_in_edge(node_id, 0);
 *                    NodeId upstream_node_id = tg.edge_src_node(in_edge);
 *
 *                    //We just copy the upstream varirable
 *                    const BDD& upstream_var = tg.node_func(upstream_node_id);
 *                    tg.set_node_func(node_id, upstream_var);
 *                    break;
 *                } case TN_Type::PRIMITIVE_OPIN: {
 *                    //Multiple in put node with non-identity logic function
 *
 *                    //Collect the upstream variables/functions
 *                    //
 *                    //NOTE: we are assuming that the input edges are in the same order as the
 *                    //      inputs in the netlist description.  This should be true since we
 *                    //      build the input edges in netlist order previously
 *                    std::vector<BDD> upstream_vars;
 *                    for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
 *                        EdgeId in_edge = tg.node_in_edge(node_id, edge_idx);
 *                        NodeId upstream_node_id = tg.edge_src_node(in_edge);
 *                        const BDD& upstream_var = tg.node_func(upstream_node_id);
 *
 *                        upstream_vars.push_back(upstream_var);
 *                    }
 *
 *                    //
 *                    //Find the blif logic function
 *                    //
 *                    
 *                    //Get the netlist port
 *                    auto iter = node_to_port_lookup.find(node_id);
 *                    assert(iter != node_to_port_lookup.end());
 *                    const BlifPort* port = iter->second;
 *
 *                    //Trace back to the primitive
 *                    assert(port->node_type == BlifNodeType::NAMES); //Currenlty we only support .names primitives
 *                    const BlifNames* names = port->names;
 *
 *                    //Build the logic function
 *                    BDD f = create_func_from_names(names, upstream_vars);
 *
 *                    //Annotate the node
 *                    tg.set_node_func(node_id, f);
 *                    break;
 *                } default: {
 *                    assert(0);
 *                }
 *            }
 *        }
 *    }
 *}
 */

BDD BlifTimingGraphBuilder::create_func_from_names(const BlifNames* names, const std::vector<BDD>& input_vars) {
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