#include <cassert>
#include <regex>
#include <unordered_map>
#include <set>
#include <queue>
#include <algorithm>
#include "BlifTimingGraphBuilder.hpp"
#include "cell_characterize.hpp"

#include <iostream>
using std::cout;

#include "bdd.hpp"

BlifTimingGraphBuilder::BlifTimingGraphBuilder(BlifData* data, const sdfparse::DelayFile& sdf_data)
    : blif_data_(data) 
    , sdf_data_(sdf_data) {
    
    std::regex interconnect_regex("routing_segment_(.*)_output_([[:digit:]]+)_([[:digit:]]+)_to_(.*)_(input|clock)_([[:digit:]]+)_([[:digit:]]+)", std::regex::egrep);

    for(const auto& cell : sdf_data_.cells()) {
        std::string inst_name = cell.instance();
        sdf_cells_by_inst_name_[inst_name] = cell;

        if(cell.celltype() == "fpga_interconnect") {
            //Extract the driver and sink port names
            
            std::smatch matches;
            bool matched = std::regex_match(inst_name, matches, interconnect_regex);

            assert(matched);

            std::string driver_port_name = matches[1];

            std::string sink_port_name = matches[4];

            auto ret = sdf_interconnect_cells_.insert(std::make_pair(std::make_tuple(driver_port_name, sink_port_name), cell));
            assert(ret.second);
        }
    }
}

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

    const BlifModel* top_model = blif_data_->get_top_model();

    cout << "Blif Info: \n";
    cout << "\tInputs : " << top_model->inputs.size() << "\n";
    cout << "\tOutputs: " << top_model->outputs.size() << "\n";
    cout << "\tNames  : " << top_model->names.size() << "\n";
    cout << "\tLatches: " << top_model->latches.size() << "\n";
    cout << "\tSubckts: " << top_model->subckts.size() << "\n";
    cout << "\tNets   : " << top_model->nets.size() << "\n";

    identify_clock_drivers();

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

    //Label all the BDD variables
    g_cudd.clearVariableNames();
    for(int i = 0; i < g_cudd.ReadSize(); i++) {
        g_cudd.pushVariableName(std::string("x") + std::to_string(i));
        //std::cout << g_cudd.getVariableName(i) << "\n";
    }

    /*
     * Once we have processed every primtiive, we then
     * walk through all the nets (external to primitives)
     * in the netlist and add edges to represent them.
     */
    create_net_edges(tg);

    //Now that we have all the edges in the graph we can levelize it
    tg.levelize();

    //check_logical_input_dependancies(tg);
    check_logical_output_dependancies(tg);

    verify(tg);
}

void BlifTimingGraphBuilder::create_input(TimingGraph& tg, const BlifPort* input_port) {
    //An input becomes the following in the timing graph:
    //    INPAD_SOURCE ---> INPAD_OPIN
    bool is_clock_source = false;
    DomainId clk_domain = 0; //Default to 1st clock domain
    auto iter = clock_driver_to_domain_.find(input_port);
    if(iter != clock_driver_to_domain_.end()) {
        is_clock_source = true;
        clk_domain = iter->second;
    }

    NodeId src;
    if(is_clock_source) {
        src = tg.add_node(TN_Type::CLOCK_SOURCE, clk_domain, is_clock_source);
    } else {
        src = tg.add_node(TN_Type::INPAD_SOURCE, clk_domain, is_clock_source); //Default to 1st clock domain
    }
    NodeId opin = tg.add_node(TN_Type::INPAD_OPIN, INVALID_CLOCK_DOMAIN, false);

    //Add the edge between them
    tg.add_edge(src, opin);

    //Identity logic function
    tg.set_node_func(src, g_cudd.bddVar(0));
    tg.set_node_func(opin, g_cudd.bddVar(0));

    //Record the mapping from blif netlist to timing graph nodes
    assert(port_to_node_lookup_.find(input_port) == port_to_node_lookup_.end()); //No found
    port_to_node_lookup_[input_port] = opin;
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
    assert(port_to_node_lookup_.find(output_port) == port_to_node_lookup_.end()); //No found
    port_to_node_lookup_[output_port] = ipin;
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
    EdgeId d_to_sink_edge_id = tg.add_edge(ipin, sink);
    EdgeId src_to_q_edge_id = tg.add_edge(src, opin);
    tg.add_edge(clock, sink);
    tg.add_edge(clock, src);


    //Identity logic function from ipin -> sink
    tg.set_node_func(ipin, g_cudd.bddVar(0));
    tg.set_node_func(sink, g_cudd.bddVar(0));

    //Identity logic function from source -> opin
    tg.set_node_func(src, g_cudd.bddVar(0));
    tg.set_node_func(opin, g_cudd.bddVar(0));

    //Record the mapping from blif netlist to timing graph nodes
    port_to_node_lookup_[latch->input] = ipin;
    port_to_node_lookup_[latch->output] = opin;
    port_to_node_lookup_[latch->control] = clock;

    set_latch_edge_delays_from_sdf(tg, latch, d_to_sink_edge_id, src_to_q_edge_id);
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

    //Skip disconnected .names
    assert(names->ports.size() > 0);

    const BlifPort* output_port = names->ports[names->ports.size()-1];

    if(names->ports.size() == 1 && output_port->port_conn == nullptr) {
        //This is a floating .names with no inputs or outputs connected
        //Do not generate any nodes for it.
        return;
    }
     
    //Build the nodes
    std::vector<NodeId> input_ids;
    std::vector<BDD> input_vars;
    for(size_t i = 0; i < names->ports.size() - 1; i++) {
        const BlifPort* input_port = names->ports[i];

        NodeId node_id = tg.add_node(TN_Type::PRIMITIVE_IPIN, INVALID_CLOCK_DOMAIN, false);
        input_ids.push_back(node_id);

        input_vars.push_back(g_cudd.bddVar(i));

        //Record the mapping from blif netlist to timing graph nodes
        port_to_node_lookup_[input_port] = node_id;
    }

    NodeId output_node_id;
    if(input_ids.size() == 0) {
        //Constant generator, with connected output
        output_node_id = tg.add_node(TN_Type::CONSTANT_GEN_SOURCE, INVALID_CLOCK_DOMAIN, false);
        std::cout << "Creating CONST_GEN_SOURCE node " << output_node_id << " outport " << *output_port->name << std::endl;
    } else {
        //Regular combinational primitive
        output_node_id = tg.add_node(TN_Type::PRIMITIVE_OPIN, INVALID_CLOCK_DOMAIN, false);
    }

    //Record the mapping from blif netlist to timing graph nodes
    port_to_node_lookup_[output_port] = output_node_id;

    //Define the opin logic function in terms of its input edges
    BDD opin_node_func = create_func_from_names(names, input_vars);
    tg.set_node_func(output_node_id, opin_node_func);

    //Build the internal edges
    for(NodeId input_node_id : input_ids) {
        tg.add_edge(input_node_id, output_node_id);
    }

    set_names_edge_delays_from_sdf(tg, names, output_node_id, opin_node_func);
}

void BlifTimingGraphBuilder::create_subckt(TimingGraph& tg, const BlifSubckt* subckt) {
    /*
     * A blif .subckt is used to introduce hierarchy and corresponds the instantiation 
     * of another model.
     *
     * We currently support only flat multi-input to multi-output combinational .subckts 
     * which are flattened in the timing graph.
     *
     * We currently assume a flat hierarhcy with a single .names driving each output,
     * and each output depending on all inputs
     */
    BlifModel* subckt_model = blif_data_->find_model(subckt->type);

    assert(subckt_model->subckts.size() == 0); //No internal hierarhcy
    assert(subckt_model->latches.size() == 0); //Combinational

    assert(subckt_model->names.size() == subckt_model->outputs.size());

    //Build the nodes
    std::vector<NodeId> input_ids;
    std::vector<BDD> input_vars;
    for(size_t i = 0; i < subckt_model->inputs.size(); i++) {
        const BlifPort* input_port = find_subckt_port_from_model_port(subckt, subckt_model->inputs[i]);

        NodeId node_id = tg.add_node(TN_Type::PRIMITIVE_IPIN, INVALID_CLOCK_DOMAIN, false);
        input_ids.push_back(node_id);

        input_vars.push_back(g_cudd.bddVar(i));

        //Record the mapping from blif netlist to timing graph nodes
        std::cout << "Subckt: " << *subckt->type << " Adding input port: " << input_port << "(" << *input_port->name << ") -> NodeID: " << node_id << std::endl;
        port_to_node_lookup_[input_port] = node_id;
    }

    std::vector<NodeId> output_ids;
    for(size_t i = 0; i < subckt_model->outputs.size(); i++) {
        const BlifPort* output_port = find_subckt_port_from_model_port(subckt, subckt_model->outputs[i]);

        NodeId node_id;
        if(input_ids.size() == 0) {
            //Constant generator
            node_id = tg.add_node(TN_Type::CONSTANT_GEN_SOURCE, INVALID_CLOCK_DOMAIN, false);
        } else {
            //Regular combinational primitive
            node_id = tg.add_node(TN_Type::PRIMITIVE_OPIN, INVALID_CLOCK_DOMAIN, false);
        }
        output_ids.push_back(node_id);

        //Record the mapping from blif netlist to timing graph nodes
        std::cout << "Subckt: " << *subckt->type << " Adding output port: " << output_port << "(" << *output_port->name << ") -> NodeID: " << node_id << std::endl;
        port_to_node_lookup_[output_port] = node_id;

        //Define the opin logic function in terms of its input edges
        BDD opin_node_func = create_func_from_names(subckt_model->names[i], input_vars);
        tg.set_node_func(node_id, opin_node_func);
    }



    //Build the internal edges
    for(size_t i = 0; i < subckt_model->inputs.size(); ++i) {
        NodeId input_node_id = input_ids[i];
        for(size_t j = 0; j < subckt_model->outputs.size(); ++j) {
            NodeId output_node_id = output_ids[j];

            tg.add_edge(input_node_id, output_node_id);
        }
    }
}

void BlifTimingGraphBuilder::create_net_edges(TimingGraph& tg) {
    /*
     * Walk through each net connecting drivers to sinks.
     *
     * We also collect equivalent variables, which we tell
     * the BDD package to re-map.
     */
    const BlifModel* top_model = blif_data_->get_top_model();

    for(const BlifNet* net : top_model->nets) {
        //cout << "Net: " << *net->name << " (" << net->sinks.size() << " sinks)\n";
        assert(net->drivers.size() == 1);
        const BlifPort* driver_port = net->drivers[0]->port;
        
        auto iter_driver = port_to_node_lookup_.find(driver_port);
        assert(iter_driver != port_to_node_lookup_.end());

        NodeId driver_node = iter_driver->second;

        for(size_t i = 0; i < net->sinks.size(); i++) {
            const BlifPort* sink_port = net->sinks[i]->port;

            auto iter_sink = port_to_node_lookup_.find(sink_port);
            assert(iter_sink != port_to_node_lookup_.end());

            NodeId sink_node = iter_sink->second;        

            //Identity logic function
            tg.set_node_func(sink_node, g_cudd.bddVar(0));

            //std::cout << "Adding edge " << driver_node << " -> " << sink_node << std::endl;
            tg.add_edge(driver_node, sink_node);

            set_net_edge_delay_from_sdf(tg, driver_port, sink_port, i, sink_node);
        }
    }

}

const BlifPort* BlifTimingGraphBuilder::find_subckt_port_from_model_port(const BlifSubckt* subckt, const BlifPort* model_port) {
    for(const auto subckt_port : subckt->ports) {
        if(model_port->name == subckt_port->name) {
            return subckt_port;
        }
    }
    assert(false);
}

/*
 *BlifPort* BlifTimingGraphBuilder::get_real_port(BlifPort* port) {
 *    BlifPort* real_port = port;
 *    if(port->node_type == BlifNodeType::SUBCKT) {
 *        BlifModel* model = blif_data_->find_model(port->subckt->type);
 *
 *        //Check if it is an input
 *        real_port = model->find_input_port(port->name);
 *
 *        //Check if it is an output
 *        if(!real_port) {
 *            real_port = model->find_output_port(port->name);
 *        }
 *
 *        //Check if it is a clock
 *        if(!real_port) {
 *            real_port = model->find_clock_port(port->name);
 *        }
 *        assert(real_port);
 *    }
 *    return real_port;
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

    //We initially make the function false, since we assume we are adding the
    //ON set from the .names
    BDD f = g_cudd.bddZero();

    //Check if we are actually doing the OFF set instead
    if(names->cover_rows.size() > 0) {
        //There is at-least one row
        //
        //Check the output value of the row to determine if this .names
        //is encoded with the ON set ('1') or OFF set ('0')
        //
        //This should be consistent through out the .names (i.e. all output values '1' or '0')
        std::vector<LogicValue>* row = names->cover_rows[0];

        LogicValue output_value = (*row)[row->size()-1];

        if(output_value == LogicValue::FALSE) {
            //This .names encodes the OFF set, so we set the 'background'
            //value to true
            f = g_cudd.bddOne();
        }
    }


    for(std::vector<LogicValue>* row : names->cover_rows) {
        //We now AND together the inputs in a single row
        //to create a cube
        BDD cube = g_cudd.bddOne();

        //We expect only the on-set to be defined, (with all other cubes
        //expected to be false)
        //
        //However sometimes synthesis tools will produce zero truth values,
        //which we can safely ignore
        LogicValue row_output_val = (*row)[row->size()-1];

        //Build up the cube from the row inputs
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

        //Add the cube to the function
        if(row_output_val == LogicValue::TRUE) {
            //This cube specifies part of the on-set, so we OR together the cubes
            //This causes f's on set to be the union of all the cubes
            f |= cube;
        } else {
            assert(row_output_val == LogicValue::FALSE);
            //This cube specifies part of the off-set, so *invert* and AND together the cubes
            //This clears the minterms covered by this cube (since we default f to all minterms
            //true when handling the off-set)
            f &= !cube;
        }
    }

    return f;
}

void BlifTimingGraphBuilder::verify(const TimingGraph& tg) {
    for(NodeId node_id : tg.primary_inputs()) {
        assert(tg.num_node_in_edges(node_id) == 0);

        auto node_type = tg.node_type(node_id);
        assert(node_type == TN_Type::INPAD_SOURCE 
               || node_type == TN_Type::FF_SOURCE
               || node_type == TN_Type::CLOCK_SOURCE
               || node_type == TN_Type::CONSTANT_GEN_SOURCE);
    }

    for(NodeId node_id : tg.primary_outputs()) {
        assert(tg.num_node_out_edges(node_id) == 0);
        assert(tg.num_node_in_edges(node_id) > 0);

        auto node_type = tg.node_type(node_id);
        assert(node_type == TN_Type::OUTPAD_SINK 
               || node_type == TN_Type::FF_SINK);
    }
}

void BlifTimingGraphBuilder::identify_clock_drivers() {
    const BlifModel* top_model = blif_data_->get_top_model();

    std::set<BlifNet*> clock_nets;
    for(BlifLatch* latch : top_model->latches) {
        BlifPort* clk_port = latch->control;
        BlifPortConn* clk_port_conn = clk_port->port_conn;
        assert(clk_port_conn != nullptr);
        BlifNet* clk_net = clk_port_conn->net;
        clock_nets.insert(clk_net);
    }


    DomainId domain_id = 0;
    for(BlifNet* net : clock_nets) {
        assert(net->drivers.size() == 1);

        BlifPortConn* driver_conn = net->drivers[0];
        BlifPort* driver = driver_conn->port;
        std::cout << "Clock Net: " << *net->name << " DomainId: " << domain_id << " Driver: " << *driver->name << "\n";
        auto result = clock_driver_to_domain_.insert({driver, domain_id});
        assert(result.second); //Was inserted

        domain_id++;
    }
}

void BlifTimingGraphBuilder::check_logical_input_dependancies(const TimingGraph& tg) {
    auto node_dependancies = std::vector<std::map<NodeId,int>>(tg.num_nodes());

    for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
        auto type = tg.node_type(node_id);
        if(type == TN_Type::INPAD_SOURCE || type == TN_Type::FF_SOURCE) {
            node_dependancies[node_id][node_id]++;
        }
    }

    for(int i = 1; i < tg.num_levels(); i++) {
        for(NodeId node_id : tg.level(i)) {
            if(tg.node_type(node_id) != TN_Type::FF_SOURCE) {
                for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                    EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

                    NodeId src_node = tg.edge_src_node(edge_id);
                    for(auto kv : node_dependancies[src_node]) {
                        node_dependancies[node_id][kv.first]++;
                    }
                }
            }
        }
    }
/*
 *
 *    cout << "Primary Output Dependancies:\n";
 *    for(int i = 0; i < tg.num_levels(); i++) {
 *        for(NodeId node_id : tg.level(i)) {
 *            int total_deps = 0;
 *            for(auto kv : node_dependancies[node_id]) {
 *                total_deps += kv.second;
 *            }
 *            cout << "\tNode: " << node_id << " (" << tg.node_type(node_id) << ") " << "#Dependancies: " << total_deps << " (" << (float) total_deps / (2*tg.logical_inputs().size()) << ")\n";
 *            cout << "\t\t{\n";
 *            for(auto kv : node_dependancies[node_id]) {
 *                cout << "\t\t\tNode: " << kv.first << ", Cnt: " << kv.second << "\n";
 *            }
 *            cout << "\t\t}\n";
 *        }
 *    }
 */
}

void BlifTimingGraphBuilder::check_logical_output_dependancies(const TimingGraph& tg) {
    //cout << "PO Transitive Fan-ins\n";
    for(NodeId po_node_id : tg.primary_outputs()) {
        std::queue<NodeId> node_queue;
        std::unordered_map<NodeId,int> transitive_fanin;

        //Initialize the queue
        for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(po_node_id); edge_idx++) {
            EdgeId edge_id = tg.node_in_edge(po_node_id, edge_idx);
            NodeId src_node = tg.edge_src_node(edge_id);
            node_queue.push(src_node);
        }

        while(!node_queue.empty()) {
            NodeId node_id = node_queue.front();
            node_queue.pop();

            transitive_fanin[node_id]++;
            
            //Enqueue the fan-in of the current node
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);
                NodeId src_node = tg.edge_src_node(edge_id);
                node_queue.push(src_node);
            }
        }

        //cout << "\tNode: " << po_node_id << " (" << tg.node_type(po_node_id) << ")\n ";
        //cout << "\t\t" << "Trans Fanin Nodes: " << transitive_fanin.size() << " (" << (float) transitive_fanin.size() / (tg.num_nodes()) << ")\n";

        int pi_fanin = 0;
        for(auto kv : transitive_fanin) {
            NodeId node_id = kv.first;
            auto type = tg.node_type(node_id);
            if(type == TN_Type::FF_SOURCE || type == TN_Type::INPAD_SOURCE) {
                pi_fanin++;
            }
        }
        //cout << "\t\t" << "#PI: " << pi_fanin << " (" << (float) pi_fanin / (tg.logical_inputs().size()) << ")\n";

        //Save stats
        logical_output_dependancy_stats_[std::make_pair(transitive_fanin.size(), pi_fanin)].push_back(po_node_id);
    }
}

std::string BlifTimingGraphBuilder::sdf_name(std::string name) {
    std::vector<char> chars_to_replace = {'$', ':', '.', '[', ']'};
    for(char val : chars_to_replace) {
        std::replace(name.begin(), name.end(), val, '_');
    }
    return name;
}

void BlifTimingGraphBuilder::set_names_edge_delays_from_sdf(const TimingGraph& tg, const BlifNames* blif_names, const NodeId output_node_id, BDD opin_node_func) {

    if(tg.node_type(output_node_id) == TN_Type::CONSTANT_GEN_SOURCE) {
        //Skip constant generators since they don't show up in SDF
        return;
    }

    //Determine the sdf name for this .names 
    std::string sdf_cell_name = sdf_name("lut_" + *blif_names->ports[blif_names->ports.size()-1]->name);

    //Find the matching SDF cell by name
    const auto& cell = find_sdf_cell_inst(sdf_cell_name);

    const auto& cell_delays = cell.delay();
    const auto& iopaths = cell_delays.iopaths();

    //Characterize the logic function to identify which input/output delays to map onto each edge
    //std::cout << "Identify active transition arcs for: " << *blif_names->ports[blif_names->ports.size()-1]->name << std::endl;
    //auto active_transition_arcs = identify_active_transition_arcs(opin_node_func, tg.num_node_in_edges(output_node_id));

    //Verify the delays make sense
    assert(cell_delays.type() == sdfparse::Delay::Type::ABSOLUTE); //Only accept absolute delays
    assert(blif_names->ports.size()-1 == iopaths.size()); //Same number of inputs
    assert(iopaths.size() == (size_t) tg.num_node_in_edges(output_node_id)); //Same number of inputs
    //assert(iopaths.size() == active_transition_arcs.size()); //Same number of inputs


    //Iterate through the edges applying the delays
    for(int i = 0; i < tg.num_node_in_edges(output_node_id); ++i) {
        EdgeId edge_id = tg.node_in_edge(output_node_id, i);

        const auto& iopath = iopaths[i];
        const auto& rise_delay_val = iopath.rise();
        const auto& fall_delay_val = iopath.fall();

        //For now we expect rise and fall to be equal
        assert(rise_delay_val == fall_delay_val);

        //Apply the delay to each pair of valid transitions
        //on this edge
        std::map<TransitionType,Time> delays; //Delays for this specific edge
        for(auto output_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
            double delay = std::numeric_limits<double>::quiet_NaN();
            if(output_trans == TransitionType::HIGH || output_trans == TransitionType::LOW) {
                delay = 0.;
            } else if(output_trans == TransitionType::RISE) {
                delay = rise_delay_val.max();
            } else {
                assert(output_trans == TransitionType::FALL);
                delay = fall_delay_val.max();
            }

            auto ret = delays.insert(std::make_pair(output_trans, Time(delay)));
            assert(ret.second); //Was inserted
        }

        auto ret = edge_delays_.insert(std::make_pair(edge_id, delays));
        assert(ret.second); //Was inserted
    }

    //std::cout << "Setting node edge delays" << std::endl; 
}

void BlifTimingGraphBuilder::set_net_edge_delay_from_sdf(const TimingGraph& tg, const BlifPort* driver_port, const BlifPort* sink_port,
                                                         size_t sink_pin_idx, const NodeId output_node_id) {

    BDD opin_node_func = tg.node_func(output_node_id);

    //We need to get the output port of the sink block (which names the sink block)
    const BlifPort* sink_block_output_port = nullptr;
    std::string sink_port_prefix;
    if(sink_port->node_type == BlifNodeType::NAMES) {
        sink_port_prefix = "lut_";

        BlifNames* sink_block = sink_port->names;
        assert(sink_block->ports.size() > 0);
        sink_block_output_port = sink_block->ports[sink_block->ports.size()-1];
    } else if (sink_port->node_type == BlifNodeType::LATCH) {
        sink_port_prefix = "latch_";

        BlifLatch* sink_block = sink_port->latch;
        sink_block_output_port = sink_block->output;
    } else {
        assert(sink_port->node_type == BlifNodeType::MODEL);
        sink_block_output_port = sink_port;
    }

    std::string driver_port_prefix;
    if(driver_port->node_type == BlifNodeType::NAMES) {
        driver_port_prefix = "lut_";
    } else if(driver_port->node_type == BlifNodeType::LATCH) {
        driver_port_prefix = "latch_";
    }

    std::string driver_port_name = driver_port_prefix + sdf_name(*driver_port->name);
    std::string sink_port_name = sink_port_prefix + sdf_name(*sink_block_output_port->name);

    const auto& sdf_cell = find_sdf_interconnect(driver_port_name, sink_port_name);

    //Set the delays on this net
    std::map<TransitionType,Time> delays; //Delays for this specific edge

    const auto& sdf_delay = sdf_cell.delay();
    assert(sdf_delay.type() == sdfparse::Delay::Type::ABSOLUTE);

    const auto& sdf_iopaths = sdf_delay.iopaths();
    assert(sdf_iopaths.size() == 1); //A net edge should be a single-input single-output cell

    const auto& rise_delay_val = sdf_iopaths[0].rise();
    const auto& fall_delay_val = sdf_iopaths[0].fall();

    //For now we expect rise and fall to be equal
    assert(rise_delay_val == fall_delay_val);

    //std::cout << "Delay: " << rise_delay_val.max() << std::endl;

    //Apply the delay to each pair of valid transitions
    //on this edge
    for(auto output_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
        double delay = NAN;
        if(output_trans == TransitionType::HIGH || output_trans == TransitionType::LOW) {
            delay = 0.;
        } else if(output_trans == TransitionType::RISE) {
            delay = rise_delay_val.max();
        } else {
            assert(output_trans == TransitionType::FALL);
            delay = fall_delay_val.max();
        }

        auto ret = delays.insert(std::make_pair(output_trans, Time(delay)));
        assert(ret.second); //Was inserted
    }

    assert(tg.num_node_in_edges(output_node_id) == 1); //Logical wire connection
    EdgeId edge_id = tg.node_in_edge(output_node_id, 0);

    assert(!delays.empty());
    //std::cout << "Setting net edge delays on edge " << edge_id << ": ";
    //for(auto kv : delays) {
        //TransitionType in, out;
        //std::tie(in, out) = kv.first;
        //std::cout << in << "/" << out << "->" << kv.second << " ";
    //}
    //std::cout << std::endl;

    //Insert the delays for this edge
    auto ret = edge_delays_.insert(std::make_pair(edge_id, delays));
    assert(ret.second); //Was inserted
}

void BlifTimingGraphBuilder::set_latch_edge_delays_from_sdf(const TimingGraph& tg, const BlifLatch* latch, EdgeId d_to_sink_edge_id, EdgeId src_to_q_edge_id) {
    auto inst_name = sdf_name("latch_" + *latch->output->name);
    const auto& sdf_cell = find_sdf_cell_inst(inst_name);

    //Setup delays
    std::map<TransitionType,Time> d_to_sink_delays;

    for(const auto& setup_check : sdf_cell.timing_check().setup()) {
        for(auto output_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
            double delay = NAN;
            if(output_trans == TransitionType::HIGH || output_trans == TransitionType::LOW) {
                delay = 0.;
            } else if(output_trans == TransitionType::RISE) {
                delay = setup_check.tsu().max();
            } else {
                assert(output_trans == TransitionType::FALL);
                delay = setup_check.tsu().max();
            }
            auto ret1 = d_to_sink_delays.insert(std::make_pair(output_trans, Time(delay)));
            assert(ret1.second); //Was inserted
        }
    }
    auto ret2 = edge_delays_.insert(std::make_pair(d_to_sink_edge_id, d_to_sink_delays));
    assert(ret2.second); //Was inserted
    
    std::map<TransitionType,Time> src_to_q_delays;

    const auto& sdf_delay = sdf_cell.delay();
    assert(sdf_delay.type() == sdfparse::Delay::Type::ABSOLUTE);

    const auto& iopaths = sdf_cell.delay().iopaths();
    assert(iopaths.size() == 1);

    const auto& iopath = iopaths[0];

    assert(iopath.input().condition() == sdfparse::PortCondition::POSEDGE);
    assert(iopath.input().port() == "clock");
    assert(iopath.output().condition() == sdfparse::PortCondition::NONE);
    assert(iopath.output().port() == "Q");

    for(auto output_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
        double delay = NAN;
        if(output_trans == TransitionType::HIGH || output_trans == TransitionType::LOW) {
            delay = 0.;
        } else if(output_trans == TransitionType::RISE) {
            delay = iopath.rise().max();
        } else {
            assert(output_trans == TransitionType::FALL);
            delay = iopath.fall().max();
        }
        auto ret3 = src_to_q_delays.insert(std::make_pair(output_trans, Time(delay)));
        assert(ret3.second); //Was inserted
    }
    auto ret4 = edge_delays_.insert(std::make_pair(src_to_q_edge_id, src_to_q_delays));
    assert(ret4.second); //Was inserted
}

std::shared_ptr<TimingGraphNameResolver> BlifTimingGraphBuilder::get_name_resolver() {
    if(!name_resolver_) {

        std::unordered_map<NodeId, const BlifPort*> node_to_port_lookup;
        for(auto kv : port_to_node_lookup_) {
            node_to_port_lookup[kv.second] = kv.first;
        }

        name_resolver_ = std::make_shared<TimingGraphBlifNameResolver>(node_to_port_lookup);
    }

    return name_resolver_;
}

sdfparse::Cell BlifTimingGraphBuilder::find_sdf_interconnect(std::string driver_port_name, std::string sink_port_name) {
    auto iter = sdf_interconnect_cells_.find(std::make_tuple(driver_port_name, sink_port_name));
    assert(iter != sdf_interconnect_cells_.end());

    return iter->second;
}

sdfparse::Cell BlifTimingGraphBuilder::find_sdf_cell_inst(std::string inst_name) {
    auto iter = sdf_cells_by_inst_name_.find(inst_name);
    assert(iter != sdf_cells_by_inst_name_.end());

    return iter->second;
}
