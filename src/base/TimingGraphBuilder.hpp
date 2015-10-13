#pragma once
#include <unordered_map>
#include "TimingGraph.hpp"
#include "blif_parse.hpp"

class TimingGraphBuilder {
    public:
        virtual void build(TimingGraph& tg) = 0;
};
