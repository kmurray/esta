#pragma once
#include "assert.hpp"
#include "TimingGraph.hpp"
#include "TimingConstraints.hpp"
#include "TimingTags.hpp"
#include "BaseAnalysisMode.hpp"
#include "object_cache.hpp"
#include <iostream>
#include <unordered_set>
#include "transition_filters.hpp"

template<class BaseAnalysisMode = BaseAnalysisMode, class Tags=TimingTags>
class ExtSetupAnalysisMode : public BaseAnalysisMode {
    public:
        typedef typename Tags::Tag Tag;

        //External tag access
        const Tags& setup_data_tags(NodeId node_id) const { return setup_data_tags_[node_id]; }
        const Tags& setup_clock_tags(NodeId node_id) const { return setup_clock_tags_[node_id]; }
        //BDD build_xfunc(const TimingGraph& tg, const ExtTimingTag& tag, const NodeId node_id);

        void set_xfunc_cache_size(size_t val) { bdd_cache_.set_capacity(val); }
        void reset_xfunc_cache();
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
        void forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id, const double delay_bin_size);

        /*
         *template<class DelayCalc>
         *void backward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id);
         */
        std::unordered_set<const Tag*> identify_filtered_tags(const std::vector<const Tag*>& input_tags, BDD node_func);
        bool input_is_filtered(size_t input_idx, const std::vector<TransitionType>& input_transitions, BDD f);

        std::vector<std::vector<const Tag*>> gen_tag_permutations(const std::vector<Tags>& input_tags);
        void gen_tag_permutations_recurr(const std::vector<Tags>& input_tags, size_t var_idx, const std::vector<const Tag*>& partial_perm, std::vector<std::vector<const Tag*>>& permutations);

        Time map_to_delay_bin(Time delay, const double delay_bin_size);

        BDD apply_restriction(int var_idx, TransitionType input_trans, BDD f);
    protected:

        //Setup tag data storage
        std::vector<Tags> setup_data_tags_; //Data tags for each node [0..timing_graph.num_nodes()-1]
        std::vector<Tags> setup_clock_tags_; //Clock tags for each node [0..timing_graph.num_nodes()-1]

        //BDD variable information
        std::unordered_map<NodeId,BDD> pi_curr_bdd_vars_;
        std::unordered_map<NodeId,BDD> pi_next_bdd_vars_;

        NextStateTransitionFilter transition_filter_;

        ObjectCacheMap<std::pair<NodeId,TransitionType>,BDD> bdd_cache_;
};



//Implementation
#include "ExtSetupAnalysisMode.tpp"
