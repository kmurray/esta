#pragma once

#include <unordered_map>
#include <vector>
#include "TimingGraph.hpp"
#include "ExtTimingTag.hpp"

/** A DelayCalculator implementation which takes a vector
 *  of pre-calculated edge delays
 */
class PreCalcTransDelayCalculator {
    public:
        using EdgeDelayModelKey = TransitionType; //output trans
        using EdgeDelayModel = std::vector<std::map<EdgeDelayModelKey,Time>>;

        ///Initializes the edge delays
        ///\param edge_delays A vector specifying the delay for every edge
        PreCalcTransDelayCalculator(const EdgeDelayModel& edge_delays)
            : edge_delays_(edge_delays) 
        {}

        Time max_edge_delay(const TimingGraph& tg, EdgeId edge_id) const {
            return max_edge_delay(tg, edge_id, TransitionType::UNKOWN, TransitionType::RISE);
        }

        Time max_edge_delay(const TimingGraph& tg, EdgeId edge_id, TransitionType input_trans, TransitionType output_trans) const {
            Time delay;
            if(input_trans == TransitionType::CLOCK || output_trans == TransitionType::CLOCK) {
                delay = Time(0.);
            } else {
                auto key = output_trans;
                auto iter = edge_delays_[edge_id].find(key);
                assert(iter != edge_delays_[edge_id].end());
                delay = iter->second;
            }
            return delay;
        }


    private:
        EdgeDelayModel edge_delays_;
};
