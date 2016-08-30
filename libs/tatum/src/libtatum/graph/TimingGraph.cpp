#include "assert.hpp"
#include <iostream>
#include <stdexcept>

#include "TimingGraph.hpp"

TE_Type TimingGraph::edge_type(const EdgeId id) const {
    //TODO: faster as a LUT?
    TN_Type src = node_type(edge_src_node(id));
    TN_Type sink = node_type(edge_sink_node(id));

    if(is_opin(src) && is_ipin(sink)) return TE_Type::NET;
    if(src == TN_Type::PRIMITIVE_IPIN && sink == TN_Type::PRIMITIVE_OPIN) return TE_Type::PRIMITIVE_INTERNAL;
    if(src == TN_Type::FF_IPIN && sink == TN_Type::FF_SINK) return TE_Type::FF_IPIN_SINK;
    if(src == TN_Type::FF_SOURCE && sink == TN_Type::FF_OPIN) return TE_Type::FF_SOURCE_OPIN;
    if(src == TN_Type::FF_CLOCK && sink == TN_Type::FF_SINK) return TE_Type::FF_CLOCK_SINK;
    if(src == TN_Type::FF_CLOCK && sink == TN_Type::FF_SOURCE) return TE_Type::FF_CLOCK_SOURCE;
    if(src == TN_Type::INPAD_SOURCE && sink == TN_Type::INPAD_OPIN) return TE_Type::INPAD_INTERNAL;
    if(src == TN_Type::OUTPAD_IPIN && sink == TN_Type::OUTPAD_SINK) return TE_Type::OUTPAD_INTERNAL;
    if(src == TN_Type::CLOCK_SOURCE && sink == TN_Type::CLOCK_OPIN) return TE_Type::CLOCK_SOURCE_INTERNAL;
    if(src == TN_Type::CONSTANT_GEN_SOURCE) return TE_Type::CONSTANT;

    return TE_Type::UNKOWN;
}

NodeId TimingGraph::add_node(const TN_Type type, const DomainId clock_domain, const bool is_clk_src) {
    //Type
    node_types_.push_back(type);

    //Domain
    node_clock_domains_.push_back(clock_domain);

    //Clock source
    node_is_clock_source_.push_back(is_clk_src);

    //Ensure there is space to store the logic function
    if(node_funcs_.size() != (size_t) num_nodes()) {
        node_funcs_.resize(num_nodes());
    }

    //Edges
    node_out_edges_.push_back(std::vector<EdgeId>());
    node_in_edges_.push_back(std::vector<EdgeId>());

    NodeId node_id = node_types_.size() - 1;

    //Verify sizes
    ASSERT(node_types_.size() == node_clock_domains_.size());
    ASSERT(node_types_.size() == node_is_clock_source_.size());

    //Return the ID of the added node
    return node_id;
}

EdgeId TimingGraph::add_edge(const NodeId src_node, const NodeId sink_node) {
    //We require that the source/sink node must already be in the graph,
    //  so we can update them with thier edge references
    VERIFY(src_node < num_nodes());
    VERIFY(sink_node < num_nodes());

    //Create the edgge
    edge_src_nodes_.push_back(src_node);
    edge_sink_nodes_.push_back(sink_node);

    //Verify
    ASSERT(edge_sink_nodes_.size() == edge_src_nodes_.size());

    //The id of the new edge
    EdgeId edge_id = edge_sink_nodes_.size() - 1;

    //Update the nodes the edge references
    node_out_edges_[src_node].push_back(edge_id);
    node_in_edges_[sink_node].push_back(edge_id);

    //Return the edge id of the added edge
    return edge_id;
}

void TimingGraph::levelize() {
    //Levelizes the timing graph
    //This over-writes any previous levelization if it exists.
    //
    //Also records primary outputs

    //Clear any previous levelization
    levels_.clear();
    primary_outputs_.clear();
    logical_inputs_.clear();
    logical_outputs_.clear();
    node_levels_ = std::vector<LevelId>(num_nodes());


    //Allocate space for the first level
    levels_.resize(1);

    //Copy the number of input edges per-node
    //These will be decremented to know when all a node's upstream parents have been
    //placed in a previous level (indicating that the node goes in the current level)
    //
    //Also initialize the first level (nodes with no fanin)
    std::vector<int> node_fanin_remaining(num_nodes());
    for(NodeId node_id = 0; node_id < num_nodes(); node_id++) {
        int node_fanin = num_node_in_edges(node_id);
        TN_Type node_type_val = node_type(node_id);
        node_fanin_remaining[node_id] = node_fanin;

        if(node_fanin == 0) {
            //Add a primary input
            levels_[0].push_back(node_id);
        }


        if(is_source(node_type_val)) {
            logical_inputs_.push_back(node_id);
        }

        if(is_sink(node_type_val)) {
            logical_outputs_.push_back(node_id);
        }
                 
    }

    //Walk the graph from primary inputs (no fanin) to generate a topological sort
    //
    //We inspect the output edges of each node and decrement the fanin count of the
    //target node.  Once the fanin count for a node reaches zero it can be added
    //to the current level.
    int level_id = 0;
    bool inserted_node_in_level = true;
    while(inserted_node_in_level) { //If nothing was inserted we are finished
        inserted_node_in_level = false;

        for(const NodeId node_id : levels_[level_id]) {
            //Inspect the fanout
            for(int edge_idx = 0; edge_idx < num_node_out_edges(node_id); edge_idx++) {
                EdgeId edge_id = node_out_edge(node_id, edge_idx);
                NodeId sink_node = edge_sink_node(edge_id);

                //Decrement the fanin count
                VERIFY(node_fanin_remaining[sink_node] > 0);
                node_fanin_remaining[sink_node]--;

                //Add to the next level if all fanin has been seen
                if(node_fanin_remaining[sink_node] == 0) {
                    //Ensure there is space by allocating the next level if required
                    levels_.resize(level_id+2);

                    //Add the node to the level
                    levels_[level_id+1].push_back(sink_node);
                    
                    //Record the level of the node
                    node_levels_[sink_node] = level_id+1;

                    inserted_node_in_level = true;
                }
            }

            //Also track the primary outputs
            if(num_node_out_edges(node_id) == 0 //) {
                    && (node_type(node_id) == TN_Type::OUTPAD_SINK || node_type(node_id) == TN_Type::FF_SINK)) {
                primary_outputs_.push_back(node_id);
            }
        }
        level_id++;
    }
}

std::vector<EdgeId> TimingGraph::optimize_edge_layout() {
    //Make all edges in a level be contiguous in memory

    //Determine the edges driven by each level of the graph
    std::vector<std::vector<EdgeId>> edge_levels;
    for(int level_idx = 0; level_idx < num_levels(); level_idx++) {
        edge_levels.push_back(std::vector<EdgeId>());
        for(auto node_id : levels_[level_idx]) {
            for(int i = 0; i < num_node_out_edges(node_id); i++) {
                EdgeId edge_id = node_out_edge(node_id, i);

                //edge_id is driven by nodes in level level_idx
                edge_levels[level_idx].push_back(edge_id);
            }
        }
    }

    /*
     * Re-allocate edges to be contiguous in memory
     */

    //Maps from from original to new edge id, used to update node to edge refs
    std::vector<EdgeId> orig_to_new_edge_id(num_edges(), -1);

    //Save the old values while we write the new ones
    std::vector<NodeId> old_edge_sink_nodes_;
    std::vector<NodeId> old_edge_src_nodes_;

    //Swap them
    std::swap(old_edge_sink_nodes_, edge_sink_nodes_);
    std::swap(old_edge_src_nodes_, edge_src_nodes_);

    //Update edge to node refs
    for(auto& edge_level : edge_levels) {
        for(auto& orig_edge_id : edge_level) {
            //Write edges in the new contiguous order
            edge_sink_nodes_.push_back(old_edge_sink_nodes_[orig_edge_id]);
            edge_src_nodes_.push_back(old_edge_src_nodes_[orig_edge_id]);

            //Save the new edge id to update nodes
            orig_to_new_edge_id[orig_edge_id] = edge_sink_nodes_.size() - 1;
        }
    }

    //Update node to edge refs
    for(NodeId node_id = 0; node_id < num_nodes(); node_id++) {
        for(int edge_idx = 0; edge_idx < num_node_out_edges(node_id); edge_idx++) {
            EdgeId old_edge_id = node_out_edges_[node_id][edge_idx];
            EdgeId new_edge_id = orig_to_new_edge_id[old_edge_id];
            node_out_edges_[node_id][edge_idx] = new_edge_id;
        }
        for(int edge_idx = 0; edge_idx < num_node_in_edges(node_id); edge_idx++) {
            EdgeId old_edge_id = node_in_edges_[node_id][edge_idx];
            EdgeId new_edge_id = orig_to_new_edge_id[old_edge_id];
            node_in_edges_[node_id][edge_idx] = new_edge_id;
        }
    }

    return orig_to_new_edge_id;
}

std::vector<NodeId> TimingGraph::optimize_node_layout() {
    //Make all nodes in a level be contiguous in memory

    /*
     * Keep a map of the old and new node ids to update edges
     * and node levels later
     */
    std::vector<NodeId> orig_to_new_node_id = std::vector<NodeId>(num_nodes(), -1);
    /*
     *int cnt = 0;
     *for(int level_idx = 0; level_idx < num_levels(); level_idx++) {
     *    for(NodeId node_id : levels_[level_idx]) {
     *        orig_to_new_node_id[node_id] = cnt;
     *        cnt++;
     *    }
     *}
     */


    /*
     * Re-allocate nodes so levels are in contiguous memory
     */
    std::vector<TN_Type> old_node_types;
    std::vector<DomainId> old_node_clock_domains;
    std::vector<std::vector<EdgeId>> old_node_out_edges;
    std::vector<std::vector<EdgeId>> old_node_in_edges;
    std::vector<bool> old_node_is_clock_source;

    //Swap the values
    std::swap(old_node_types, node_types_);
    std::swap(old_node_clock_domains, node_clock_domains_);
    std::swap(old_node_out_edges, node_out_edges_);
    std::swap(old_node_in_edges, node_in_edges_);
    std::swap(old_node_is_clock_source, node_is_clock_source_);

    //Update the values
    for(int level_idx = 0; level_idx < num_levels(); level_idx++) {
        for(NodeId old_node_id : levels_[level_idx]) {
            node_types_.push_back(old_node_types[old_node_id]);
            node_clock_domains_.push_back(old_node_clock_domains[old_node_id]);
            node_out_edges_.push_back(old_node_out_edges[old_node_id]);
            node_in_edges_.push_back(old_node_in_edges[old_node_id]);
            node_is_clock_source_.push_back(old_node_is_clock_source[old_node_id]);

            //Record the new node id
            orig_to_new_node_id[old_node_id] = node_types_.size() - 1;
        }
    }

    /*
     * Update old references to node_ids with thier new values
     */
    //The node levels
    for(int level_idx = 0; level_idx < num_levels(); level_idx++) {
        for(size_t node_idx = 0; node_idx < levels_[level_idx].size(); node_idx++) {
            NodeId old_node_id = levels_[level_idx][node_idx];
            NodeId new_node_id = orig_to_new_node_id[old_node_id];

            levels_[level_idx][node_idx] = new_node_id;
        }
    }

    //The primary outputs
    for(size_t i = 0; i < primary_outputs_.size(); i++) {
        NodeId old_node_id = primary_outputs_[i];
        NodeId new_node_id = orig_to_new_node_id[old_node_id];

        primary_outputs_[i] = new_node_id;
    }

    //The Edges
    for(int i = 0; i < num_edges(); i++) {
        NodeId old_sink_node = edge_sink_nodes_[i];
        NodeId old_src_node = edge_src_nodes_[i];

        NodeId new_sink_node = orig_to_new_node_id[old_sink_node];
        NodeId new_src_node = orig_to_new_node_id[old_src_node];

        edge_sink_nodes_[i] = new_sink_node;
        edge_src_nodes_[i] = new_src_node;
    }

    return orig_to_new_node_id;
}

//Stream extraction for TN_Type
std::istream& operator>>(std::istream& is, TN_Type& type) {
    std::string tok;
    is >> tok; //Read in a token
    if(tok == "INPAD_SOURCE")               type = TN_Type::INPAD_SOURCE;
    else if (tok == "INPAD_OPIN")           type = TN_Type::INPAD_OPIN;
    else if (tok == "OUTPAD_IPIN")          type = TN_Type::OUTPAD_IPIN;
    else if (tok == "OUTPAD_SINK")          type = TN_Type::OUTPAD_SINK;
    else if (tok == "PRIMITIVE_IPIN")       type = TN_Type::PRIMITIVE_IPIN;
    else if (tok == "PRIMITIVE_OPIN")       type = TN_Type::PRIMITIVE_OPIN;
    else if (tok == "FF_IPIN")              type = TN_Type::FF_IPIN;
    else if (tok == "FF_OPIN")              type = TN_Type::FF_OPIN;
    else if (tok == "FF_SINK")              type = TN_Type::FF_SINK;
    else if (tok == "FF_SOURCE")            type = TN_Type::FF_SOURCE;
    else if (tok == "FF_CLOCK")             type = TN_Type::FF_CLOCK;
    else if (tok == "CLOCK_SOURCE")         type = TN_Type::CLOCK_SOURCE;
    else if (tok == "CLOCK_OPIN")           type = TN_Type::CLOCK_OPIN;
    else if (tok == "CONSTANT_GEN_SOURCE")  type = TN_Type::CONSTANT_GEN_SOURCE;
    else if (tok == "UNKOWN")               type = TN_Type::UNKOWN;
    else throw std::domain_error(std::string("Unrecognized TN_Type: ") + tok);
    return is;
}

//Stream output for TN_Type
std::ostream& operator<<(std::ostream& os, const TN_Type type) {
    if(type == TN_Type::INPAD_SOURCE)              os << "INPAD_SOURCE";
    else if (type == TN_Type::INPAD_OPIN)          os << "INPAD_OPIN";
    else if (type == TN_Type::OUTPAD_IPIN)         os << "OUTPAD_IPIN";
    else if (type == TN_Type::OUTPAD_SINK)         os << "OUTPAD_SINK";
    else if (type == TN_Type::PRIMITIVE_IPIN)      os << "PRIMITIVE_IPIN";
    else if (type == TN_Type::PRIMITIVE_OPIN)      os << "PRIMITIVE_OPIN";
    else if (type == TN_Type::FF_IPIN)             os << "FF_IPIN";
    else if (type == TN_Type::FF_OPIN)             os << "FF_OPIN";
    else if (type == TN_Type::FF_SINK)             os << "FF_SINK";
    else if (type == TN_Type::FF_SOURCE)           os << "FF_SOURCE";
    else if (type == TN_Type::FF_CLOCK)            os << "FF_CLOCK";
    else if (type == TN_Type::CLOCK_SOURCE)        os << "CLOCK_SOURCE";
    else if (type == TN_Type::CLOCK_OPIN)          os << "CLOCK_OPIN";
    else if (type == TN_Type::CONSTANT_GEN_SOURCE) os << "CONSTANT_GEN_SOURCE";
    else if (type == TN_Type::UNKOWN)              os << "UNKOWN";
    else throw std::domain_error("Unrecognized TN_Type");
    return os;
}

