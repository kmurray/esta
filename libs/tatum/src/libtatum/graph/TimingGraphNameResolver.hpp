#pragma once
#include <string>

#include "timing_graph_fwd.hpp"

class TimingGraphNameResolver {
    
    public:
        virtual std::string get_node_name(NodeId node_id) = 0;
};
