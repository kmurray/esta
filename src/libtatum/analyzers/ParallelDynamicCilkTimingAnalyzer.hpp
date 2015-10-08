#pragma once
#include <atomic>
#include <omp.h>

#include "TimingGraph.hpp"
#include "SerialTimingAnalyzer.hpp"


#if 0 //Currently disabled since gcc > 4.8 can't inline atomic.load() for some reason...

class ParallelDynamicCilkTimingAnalyzer : public SerialTimingAnalyzer {
    public: 
        ta_runtime calculate_timing(TimingGraph& timing_graph) override;

    private:
        /*
         * Setup the timing graph.
         *   Includes propogating clock domains and clock skews to clock pins
         */
        void pre_traversal(TimingGraph& timing_graph) override;

        /*
         * Propogate arrival times
         */
        void forward_traversal(TimingGraph& timing_graph) override;

        /*
         * Propogate required times
         */
        void backward_traversal(TimingGraph& timing_graph) override;

        //Parallel worker functions
        void forward_traverse_node(TimingGraph& tg, NodeId node_id);
        void backward_traverse_node(TimingGraph& tg, NodeId node_id);

        void create_synchronization(TimingGraph& tg);

        std::vector<std::atomic<int>> node_arrival_inputs_ready_count_;
        std::vector<std::atomic<int>> node_required_outputs_ready_count_;
};

#endif
