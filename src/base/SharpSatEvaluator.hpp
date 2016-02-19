#pragma once
#include <memory>
#include "timing_graph_fwd.hpp"
#include "ExtTimingTag.hpp"
#include "ep_real.hpp"


template<class Analyzer>
class SharpSatEvaluator {
    public:
        SharpSatEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer, int nvars)
            : tg_(tg)
            , analyzer_(analyzer)
            , nvars_(nvars) {}
        virtual ~SharpSatEvaluator() {}

        struct count_support {
            typedef real_t count_type;
            typedef std::vector<unsigned int> support_type;

            count_type count;
            support_type support;
            bool pure_bdd;


            friend std::ostream& operator<<(std::ostream& os, const count_support& cs) {
                os << "[Count: " << cs.count;
                os << " Support: {";
                for(size_t i = 0; i < cs.support.size(); i++) {
                    os << cs.support[i];
                    if(i < cs.support.size() -1) {
                        os << " ";
                    }
                }
                os << "}]";
                return os;
            }
        };

        virtual count_support count_sat(const ExtTimingTag* tag, NodeId node_id) = 0;
        virtual void reset() {}

    protected:
        const TimingGraph& tg_;
        std::shared_ptr<Analyzer> analyzer_;
        int nvars_;
};
