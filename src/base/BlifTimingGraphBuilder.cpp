#include <cassert>
#include <unordered_map>
#include "BlifTimingGraphBuilder.hpp"

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
    NodeId src = tg.add_node(TN_Type::INPAD_SOURCE, 0, false); //Default to 1st clock domain
    NodeId opin = tg.add_node(TN_Type::INPAD_OPIN, INVALID_CLOCK_DOMAIN, false);

    //Add the edge between them
    tg.add_edge(src, opin);

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
     *       PRIMTIVE_IPIN ---------PRIMITIVE_OPTIN
     *                             /
     *       PRIMTIVE_IPIN --------
     */
     
    //Build the nodes
    std::vector<NodeId> input_ids;
    for(size_t i = 0; i < names->ports.size() - 1; i++) {
        const BlifPort* input_port = names->ports[i];

        NodeId node_id = tg.add_node(TN_Type::PRIMITIVE_IPIN, INVALID_CLOCK_DOMAIN, false);
        input_ids.push_back(node_id);

        //Record the mapping from blif netlist to timing graph nodes
        port_to_node_lookup[input_port] = node_id;
    }

    const BlifPort* output_port = names->ports[names->ports.size()-1];
    NodeId output_node_id = tg.add_node(TN_Type::PRIMITIVE_OPIN, INVALID_CLOCK_DOMAIN, false);
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
     * Walk through each net connecting drivers to sinks
     */
    for(BlifNet* net : blif_data_->nets) {
        assert(net->drivers.size() == 1);
        BlifPort* driver_port= net->drivers[0]->port;
        
        auto iter_driver = port_to_node_lookup.find(driver_port);
        assert(iter_driver != port_to_node_lookup.end());

        NodeId driver_node = iter_driver->second;

        for(BlifPortConn* sink_conn : net->sinks) {
            BlifPort* sink_port = sink_conn->port;

            auto iter_sink = port_to_node_lookup.find(sink_port);
            assert(iter_sink != port_to_node_lookup.end());

            NodeId sink_node = iter_sink->second;        

            tg.add_edge(driver_node, sink_node);
        }
    }
}
