#include "cilk_safe.hpp"
#include <cilk/cilk_api.h>

#include "ParallelLevelizedTimingAnalyzer.hpp"
#include "TimingGraph.hpp"

template<class AnalysisType, class DelayCalcType>
ParallelLevelizedTimingAnalyzer<AnalysisType, DelayCalcType>::ParallelLevelizedTimingAnalyzer(const TimingGraph& timing_graph, const TimingConstraints& timing_constraints, const DelayCalcType& delay_calculator)
    : SerialTimingAnalyzer<AnalysisType,DelayCalcType>(timing_graph, timing_constraints, delay_calculator)
    //These thresholds control how fine-grained we allow the parallelization
    //
    //The ideal values will vary on different machines, however default value
    // is not set too aggressivley, and should be reasonable on other systems.
    //
    //These values were set experimentally on an Intel E5-1620 CPU,
    // and yeilded the following self-relative parallel speed-ups
    // compared to a value of 0 (i.e. always run parrallel).
    //
    //                  0           400
    // ====================================
    // bitcoin          3.17x       3.23x       (+2%)
    // gaussianblur     2.25x       2.83x       (+25%)
    //
    //NOTE:
    //   The gaussinablur benchmark has a very tall and narrow timing
    //   graph (i.e. many small levels which parallelize poorly). In
    //   contrast, bitcoin is relatively short and wide (i.e. parallelizes well)
    //
    //The threshold value 400 was choosen to avoid the poor parallelization on small levels,
    //while not decreasing the amount of useful parallelism on larger levels.
    , parallel_threshold_fwd_(400)
    , parallel_threshold_bck_(400) {
}

template<class AnalysisType, class DelayCalcType>
ParallelLevelizedTimingAnalyzer<AnalysisType, DelayCalcType>::~ParallelLevelizedTimingAnalyzer() {
}

template<class AnalysisType, class DelayCalcType>
void ParallelLevelizedTimingAnalyzer<AnalysisType, DelayCalcType>::forward_traversal() {
    using namespace std::chrono;

    //Forward traversal (arrival times)
    for(LevelId level_id = 1; level_id < this->tg_.num_levels(); level_id++) {
        auto fwd_level_start = high_resolution_clock::now();

        const std::vector<NodeId>& level = this->tg_.level(level_id);

        if(level.size() > parallel_threshold_fwd_) {
            //Parallel
            cilk_for(int node_idx = 0; node_idx < level.size(); node_idx++) {
                NodeId node_id = level[node_idx];

                this->forward_traverse_node(node_id);
            }
        } else {
            //Serial
            for(int node_idx = 0; node_idx < (int) level.size(); node_idx++) {
                NodeId node_id = level[node_idx];
                this->forward_traverse_node(node_id);
            }

        }

        auto fwd_level_end = high_resolution_clock::now();
        std::string key = std::string("fwd_level_") + std::to_string(level_id);
        this->perf_data_[key] = duration_cast<duration<double>>(fwd_level_end - fwd_level_start).count();
    }
}

template<class AnalysisType, class DelayCalcType>
void ParallelLevelizedTimingAnalyzer<AnalysisType, DelayCalcType>::backward_traversal() {
    using namespace std::chrono;

    //Backward traversal (required times)
    for(LevelId level_id = this->tg_.num_levels() - 2; level_id >= 0; level_id--) {
        auto bck_level_start = high_resolution_clock::now();

        const std::vector<NodeId>& level = this->tg_.level(level_id);

        if(level.size() > parallel_threshold_bck_) {
            //Parallel
            cilk_for(int node_idx = 0; node_idx < level.size(); node_idx++) {
                NodeId node_id = level[node_idx];

                this->backward_traverse_node(node_id);
            }
        } else {
            //Serial
            for(int node_idx = 0; node_idx < (NodeId) level.size(); node_idx++) {
                NodeId node_id = level[node_idx];
                this->backward_traverse_node(node_id);
            }
        }

        auto bck_level_end = high_resolution_clock::now();
        std::string key = std::string("bck_level_") + std::to_string(level_id);
        this->perf_data_[key] = duration_cast<duration<double>>(bck_level_end - bck_level_start).count();
    }
}
