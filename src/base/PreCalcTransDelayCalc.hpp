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

        ///Initializes the edge delays
        ///\param edge_delays A vector specifying the delay for every edge
        PreCalcTransDelayCalculator(std::map<TransitionType,std::vector<float>>& edge_delays) {
            for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW, TransitionType::CLOCK}) {
                edge_delays_[trans].reserve(edge_delays.size());
                for(float delay : edge_delays[trans]) {
                    edge_delays_[trans].emplace_back(delay);
                }
            }
        }

        Time min_edge_delay(const TimingGraph& tg, EdgeId edge_id, TransitionType trans) const { 
            return max_edge_delay(tg, edge_id, trans);
        };
        Time max_edge_delay(const TimingGraph& tg, EdgeId edge_id, TransitionType trans) const {
            auto iter = edge_delays_.find(trans);
            assert(iter != edge_delays_.end());
            const std::vector<Time>& trans_delays = iter->second;
            return trans_delays[edge_id];
        };
    private:
        std::map<TransitionType,std::vector<Time>> edge_delays_;
};
