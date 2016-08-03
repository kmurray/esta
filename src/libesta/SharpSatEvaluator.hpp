#pragma once
#include <memory>
#include "timing_graph_fwd.hpp"
#include "ExtTimingTag.hpp"

template<class Analyzer>
class SharpSatEvaluator {
    public:
        SharpSatEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer, size_t nvars)
            : tg_(tg)
            , analyzer_(analyzer)
            , nvars_(nvars) {}
        virtual ~SharpSatEvaluator() {}

        virtual double count_sat_fraction(const ExtTimingTag* tag) = 0;
        virtual void reset() {}

    protected:
        const TimingGraph& tg_;
        std::shared_ptr<Analyzer> analyzer_;
        size_t nvars_;
};
