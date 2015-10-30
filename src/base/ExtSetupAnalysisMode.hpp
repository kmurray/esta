#pragma once
#include "assert.hpp"
#include "TimingGraph.hpp"
#include "TimingConstraints.hpp"
#include "TimingTags.hpp"
#include "BaseAnalysisMode.hpp"
#include "object_cache.hpp"
#include <iostream>

template<class BaseAnalysisMode = BaseAnalysisMode, class Tags=TimingTags>
class ExtSetupAnalysisMode : public BaseAnalysisMode {
    public:
        typedef typename Tags::Tag Tag;

        //External tag access
        const Tags& setup_data_tags(NodeId node_id) const { return setup_data_tags_[node_id]; }
        const Tags& setup_clock_tags(NodeId node_id) const { return setup_clock_tags_[node_id]; }
        BDD build_xfunc(const TimingGraph& tg, const ExtTimingTag& tag, const NodeId node_id);

        void set_xfunc_cache_size(size_t val) { bdd_cache_.set_capacity(val); }
        void reset_xfunc_cache() { bdd_cache_.print_stats(); bdd_cache_.clear(); bdd_cache_.reset_stats(); }
    protected:
        //Internal operations for performing setup analysis to satisfy the BaseAnalysisMode interface
        void initialize_traversal(const TimingGraph& tg);

        template<class DelayCalc>
        void pre_traverse_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id);

        /*
         *template<class DelayCalc>
         *void forward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id);
         */

        template<class DelayCalc>
        void forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id);

        /*
         *template<class DelayCalc>
         *void backward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id);
         */

        BDD generate_pi_switch_func(NodeId node_id, TransitionType trans);

        std::vector<std::vector<Tag>> gen_tag_permutations(const std::vector<Tags>& input_tags);
        void gen_tag_permutations_recurr(const std::vector<Tags>& input_tags, size_t var_idx, const std::vector<Tag>& partial_perm, std::vector<std::vector<Tag>>& permutations);
        TransitionType evaluate_transition(const std::vector<Tag>& input_tags_scenario, const BDD& node_func);

        //Setup tag data storage
        std::vector<Tags> setup_data_tags_; //Data tags for each node [0..timing_graph.num_nodes()-1]
        std::vector<Tags> setup_clock_tags_; //Clock tags for each node [0..timing_graph.num_nodes()-1]

        //BDD variable information
        std::unordered_map<NodeId,BDD> pi_curr_bdd_vars_;
        std::unordered_map<NodeId,BDD> pi_next_bdd_vars_;

        ObjectCacheMap<std::pair<NodeId,TransitionType>,BDD> bdd_cache_;
};



//Implementation
#include "ExtSetupAnalysisMode.tpp"
