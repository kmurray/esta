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
        using EdgeDelayModelKey = std::tuple<TransitionType,TransitionType>; //EdgId, Input trans, output trans
        using EdgeDelayModel = std::vector<std::map<EdgeDelayModelKey,Time>>;

        ///Initializes the edge delays
        ///\param edge_delays A vector specifying the delay for every edge
        PreCalcTransDelayCalculator(const EdgeDelayModel& edge_delays)
            : edge_delays_(edge_delays) 
        {}

        Time max_edge_delay(const TimingGraph& tg, EdgeId edge_id, TransitionType input_trans, TransitionType output_trans) const {
            Time delay;
            if(input_trans == TransitionType::CLOCK || output_trans == TransitionType::CLOCK) {
                delay = Time(0.);
            } else {
                auto iter = edge_delays_[edge_id].find(std::make_tuple(input_trans, output_trans));
                assert(iter != edge_delays_[edge_id].end());
                delay = iter->second;
            }
            return delay;
        }


    private:
        EdgeDelayModel edge_delays_;
};
