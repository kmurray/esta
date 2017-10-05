#pragma once
#include <memory>
#include "timing_graph_fwd.hpp"
#include "ExtTimingTag.hpp"

template<class Analyzer>
class SharpSatEvaluator {
    public:
        SharpSatEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer)
            : tg_(tg)
            , analyzer_(analyzer) {}
        virtual ~SharpSatEvaluator() {}

        virtual double count_sat_fraction(ExtTimingTag::cptr tag) = 0;
        virtual void reset() {}

    protected:
        const TimingGraph& tg_;
        std::shared_ptr<Analyzer> analyzer_;
};
