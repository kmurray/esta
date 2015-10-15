#pragma once
#include "assert.hpp"
#include "TimingGraph.hpp"
#include "TimingConstraints.hpp"
#include "TimingTags.hpp"
#include "BaseAnalysisMode.hpp"

template<class BaseAnalysisMode = BaseAnalysisMode, class Tags=TimingTags>
class ExtSetupAnalysisMode : public BaseAnalysisMode {
    public:
        typedef typename Tags::Tag Tag;
        //External tag access
        const Tags& setup_data_tags(NodeId node_id) const { return setup_data_tags_[node_id]; }
        const Tags& setup_clock_tags(NodeId node_id) const { return setup_clock_tags_[node_id]; }
    protected:
        //Internal operations for performing setup analysis to satisfy the BaseAnalysisMode interface
        void initialize_traversal(const TimingGraph& tg);

        void pre_traverse_node(const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id);

        template<class DelayCalc>
        void forward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id);

        void forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id);

        template<class DelayCalc>
        void backward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id);

        //Setup tag data storage
        std::vector<Tags> setup_data_tags_; //Data tags for each node [0..timing_graph.num_nodes()-1]
        std::vector<Tags> setup_clock_tags_; //Clock tags for each node [0..timing_graph.num_nodes()-1]
};



//Implementation
#include "ExtSetupAnalysisMode.tpp"
