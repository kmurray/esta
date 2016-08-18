#include <limits>
#include <algorithm>

template<class AnalysisType, class DelayCalcType>
SerialTimingAnalyzer<AnalysisType,DelayCalcType>::SerialTimingAnalyzer(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc, const TagReducer& tag_reducer, size_t max_permutations)
    : tg_(tg)
    , tc_(tc)
    , dc_(dc)
    , tag_reducer_(tag_reducer)
    , max_permutations_(max_permutations) {
    AnalysisType::initialize_traversal(tg_);
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::calculate_timing() {
    using namespace std::chrono;

    auto analysis_start = high_resolution_clock::now();

    auto pre_traversal_start = high_resolution_clock::now();
    pre_traversal();
    auto pre_traversal_end = high_resolution_clock::now();

    auto fwd_traversal_start = high_resolution_clock::now();
    forward_traversal();
    auto fwd_traversal_end = high_resolution_clock::now();

    auto bck_traversal_start = high_resolution_clock::now();
    backward_traversal();
    auto bck_traversal_end = high_resolution_clock::now();

    auto analysis_end = high_resolution_clock::now();

    //Convert time points to durations and store
    perf_data_["analysis"] = duration_cast<duration<double>>(analysis_end - analysis_start).count();
    perf_data_["pre_traversal"] = duration_cast<duration<double>>(pre_traversal_end - pre_traversal_start).count();
    perf_data_["fwd_traversal"] = duration_cast<duration<double>>(fwd_traversal_end - fwd_traversal_start).count();
    perf_data_["bck_traversal"] = duration_cast<duration<double>>(bck_traversal_end - bck_traversal_start).count();
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::reset_timing() {
    AnalysisType::initialize_traversal(tg_);
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::pre_traversal() {
    /*
     * The pre-traversal sets up the timing graph for propagating arrival
     * and required times.
     * Steps performed include:
     *   - Initialize arrival times on primary inputs
     */
    for(NodeId node_id : tg_.primary_inputs()) {
        AnalysisType::pre_traverse_node(tg_, tc_, dc_, node_id);
    }
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::forward_traversal() {
    using namespace std::chrono;

    //Forward traversal (arrival times)
    for(LevelId level_id = 1; level_id < tg_.num_levels(); level_id++) {
        auto fwd_level_start = high_resolution_clock::now();

        std::cout << "\tLevel " << level_id << " ";
        const auto& level = tg_.level(level_id);

        double total_level_tags = 0.;
        double min_level_tags = std::numeric_limits<double>::max();
        double max_level_tags = 0.;

        double total_permutations = 0.;
        double min_permutations = std::numeric_limits<double>::max();
        double max_permutations = 0.;

        for(size_t i = 0; i < level.size(); ++i) {
            std::cout << ".";
            std::cout.flush();
            NodeId node_id = level[i];

            forward_traverse_node(node_id);
            
            //Stats
            total_level_tags += this->setup_data_tags_[node_id].num_tags();
            min_level_tags = std::min(min_level_tags, (double) this->setup_data_tags_[node_id].num_tags());
            max_level_tags = std::max(max_level_tags, (double) this->setup_data_tags_[node_id].num_tags());

            double node_perms = 1.;
            for(int iedge = 0; iedge < tg_.num_node_in_edges(node_id); iedge++) {
                EdgeId edge_id = tg_.node_in_edge(node_id, iedge);
                NodeId src_node_id = tg_.edge_src_node(edge_id);
                
                node_perms *= this->setup_data_tags_[src_node_id].num_tags();
            }
            total_permutations += node_perms;
            min_permutations = std::min(min_permutations, node_perms);
            max_permutations = std::max(max_permutations, node_perms);
        }
        std::cout << std::endl;

        std::cout << "\tLevel " << level_id << " Tags:";
        std::cout << " Avg: " << total_level_tags / level.size();
        std::cout << " Min: " << min_level_tags;
        std::cout << " Max: " << max_level_tags;
        std::cout << std::endl;

        std::cout << "\tLevel " << level_id << " Permutations:";
        std::cout << " Total: " << total_permutations;
        std::cout << " Avg: " << total_permutations / level.size();
        std::cout << " Min: " << min_permutations;
        std::cout << " Max: " << max_permutations;
        std::cout << std::endl;


        auto fwd_level_end = high_resolution_clock::now();
        std::string key = std::string("fwd_level_") + std::to_string(level_id);
        perf_data_[key] = duration_cast<duration<double>>(fwd_level_end - fwd_level_start).count();
    }
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::backward_traversal() {
    using namespace std::chrono;

    //Backward traversal (required times)
    for(LevelId level_id = tg_.num_levels() - 2; level_id >= 0; level_id--) {
        auto bck_level_start = high_resolution_clock::now();

        for(NodeId node_id : tg_.level(level_id)) {
            backward_traverse_node(node_id);
        }

        auto bck_level_end = high_resolution_clock::now();
        std::string key = std::string("bck_level_") + std::to_string(level_id);
        perf_data_[key] = duration_cast<duration<double>>(bck_level_end - bck_level_start).count();
    }
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::forward_traverse_node(const NodeId node_id) {
    //Pull from upstream sources to current node
    for(int edge_idx = 0; edge_idx < tg_.num_node_in_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg_.node_in_edge(node_id, edge_idx);

        AnalysisType::forward_traverse_edge(tg_, tc_, dc_, node_id, edge_id);
    }

    AnalysisType::forward_traverse_finalize_node(tg_, tc_, dc_, node_id, tag_reducer_, max_permutations_);
}

template<class AnalysisType, class DelayCalcType>
void SerialTimingAnalyzer<AnalysisType,DelayCalcType>::backward_traverse_node(const NodeId node_id) {
    //Pull from downstream sinks to current node

    //We don't propagate required times past FF_CLOCK nodes,
    //since anything upstream is part of the clock network
    //
    //TODO: if performing optimization on a clock network this may actually be useful
    if(tg_.node_type(node_id) == TN_Type::FF_CLOCK) {
        return;
    }

    //Each back-edge from down stream node
    for(int edge_idx = 0; edge_idx < tg_.num_node_out_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg_.node_out_edge(node_id, edge_idx);

        AnalysisType::backward_traverse_edge(tg_, tc_, dc_, node_id, edge_id);
    }

    AnalysisType::backward_traverse_finalize_node(tg_, tc_, dc_, node_id);
}

