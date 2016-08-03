#pragma once
#include "timing_graph_fwd.hpp"
#include "TimingGraphNameResolver.hpp"
#include "blif_data.hpp"

class TimingGraphBlifNameResolver : public TimingGraphNameResolver {
    public:
        TimingGraphBlifNameResolver(std::unordered_map<NodeId,const BlifPort*> node_to_port_lookup)
            : node_to_port_lookup_(node_to_port_lookup)
            {}

        std::string get_node_name(NodeId node_id) {
            auto iter = node_to_port_lookup_.find(node_id);
            if(iter != node_to_port_lookup_.end()) {
                const BlifPort* port = iter->second;
                return *port->name;
            }
            return "<unkown>";
        }

    private:
        std::unordered_map<NodeId,const BlifPort*> node_to_port_lookup_;
};
