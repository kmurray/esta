#include <limits>
#include <algorithm>

template<class AnalysisType, class DelayCalcType>
SerialTimingAnalyzer<AnalysisType,DelayCalcType>::SerialTimingAnalyzer(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc)
    : tg_(tg)
    , tc_(tc)
    , dc_(dc) {
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

        //Track extra statistics
        float xfunc_nnodes_total = 0;
        float xfunc_nnodes_min = std::numeric_limits<float>::max();
        float xfunc_nnodes_max = std::numeric_limits<float>::lowest();
        float xfunc_nvars_total = 0;
        float xfunc_nvars_min = std::numeric_limits<float>::max();
        float xfunc_nvars_max = std::numeric_limits<float>::lowest();
        int ntags = 0;

        std::cout << "\tLevel " << level_id << "..." << std::endl;
        for(NodeId node_id : tg_.level(level_id)) {
            /*std::cout << "\t\tNode " << node_id << std::endl;*/
            forward_traverse_node(node_id);

            //Collect statistics
            for(const auto& tag : this->setup_data_tags_[node_id]) {
                auto xfunc = tag.switch_func();

                float nnodes = xfunc.nodeCount();
                float nvars = xfunc.SupportSize();
                xfunc_nnodes_total += nnodes;
                xfunc_nnodes_min = std::min(xfunc_nnodes_min, nnodes);
                xfunc_nnodes_max = std::max(xfunc_nnodes_max, nnodes);
                xfunc_nvars_total += nvars;
                xfunc_nvars_min = std::min(xfunc_nvars_min, nvars);
                xfunc_nvars_max = std::max(xfunc_nvars_max, nvars);
                ntags++;
            }
        }

        std::cout << "\t\tmin_nnodes: " << xfunc_nnodes_min << " avg_nnodes: " << xfunc_nnodes_total / ntags << " max_nnodes: " << xfunc_nnodes_max << " nnodes_total: " << xfunc_nnodes_total << "\n";
        std::cout << "\t\tmin_nvars : " << xfunc_nvars_min << " avg_nvars : " << xfunc_nvars_total / ntags << " (" << (xfunc_nvars_total / ntags) / (2*tg_.logical_inputs().size()) << ")" << " max_nvars : " << xfunc_nvars_max << "\n";


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

    AnalysisType::forward_traverse_finalize_node(tg_, tc_, dc_, node_id);
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

