#pragma once
#include <memory>

#include "SharpSatEvaluator.hpp"
#include "CuddSharpSatFraction.h"

#define USE_BDD_CACHE

//#define BDD_CALC_DEBUG
//#define DEBUG_PRINT_MINTERMS

template<class Analyzer>
class SharpSatBddEvaluator : public SharpSatEvaluator<Analyzer> {
    private:
        typedef ObjectCacheMap<ExtTimingTag::cptr,BDD> BddCache;
    public:
        SharpSatBddEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer, size_t nvars)
            : SharpSatEvaluator<Analyzer>(tg, analyzer, nvars) {
            //We have a unique logic variable for each Primary Input
            //
            //To represent transitions we have both a 'curr' and 'next' variable
            pi_curr_bdd_vars_.clear();
            pi_next_bdd_vars_.clear();
            for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
                auto node_type = tg.node_type(node_id);
                if(node_type == TN_Type::INPAD_SOURCE || node_type == TN_Type::FF_SOURCE) {
                    //Generate the current variable
                    pi_curr_bdd_vars_[node_id] = g_cudd.bddVar();
                    g_cudd.pushVariableName("n" + std::to_string(node_id));

                    //We need to generate and record a new 'next' variable
                    pi_next_bdd_vars_[node_id] = g_cudd.bddVar();
                    g_cudd.pushVariableName("n" + std::to_string(node_id) + "'");
                } else if(node_type == TN_Type::CONSTANT_GEN_SOURCE) {
                    const_gens_.insert(node_id);
                }
            }

            assert(pi_curr_bdd_vars_.size() + pi_curr_bdd_vars_.size() == nvars);
        }

        double count_sat_fraction(ExtTimingTag::cptr tag) override {
            BDD f = build_bdd_xfunc(tag);

            return bdd_sharpsat_fraction(f);
        }

        void reset() override { 
            int i = 0;
            for(auto& cudd : {g_cudd}) {
                std::cout << "Cudd " << i << " (" << cudd.getManager() << ")\n";
                std::cout << "\tnvars       : " << cudd.ReadSize() << "\n";
                std::cout << "\tpeak_nnodes : " << cudd.ReadPeakNodeCount() << "\n";
                std::cout << "\tcurr_nnodes : " << cudd.ReadNodeCount() << "\n";
                std::cout << "\tnum_reorders: " << cudd.ReadReorderings() << "\n";
                std::cout << "\treorder_time: " << (float) cudd.ReadReorderingTime() / 1000 << "\n";
                i++;
            }
            bdd_cache_ = BddCache();

            //Reset the default re-order size
            //Re-ordering really big BDDs is slow (re-order time appears to be quadratic in size)
            //So cap the size for re-ordering to something reasonable when re-starting
            /*
             *auto next_reorder = std::min(g_cudd.ReadNextReordering(), 100004u);
             *g_cudd.SetNextReordering(next_reorder);
             */

             g_cudd.SetNextReordering(4096);
        }

        BDD build_bdd_xfunc(ExtTimingTag::cptr tag, int level=0) {
            /*std::cout << "build_xfunc at Node: " << node_id << " TAG: " << tag << "\n";*/
            auto key = tag;

#ifdef BDD_CALC_DEBUG
            //Indent based on recursion level 
            std::string tab(level, ' ');
            std::cout << tab << "Requested BDD for " << node_id << " " << tag << " " << this->tg_.node_type(node_id) << "\n";
#endif

            if(!bdd_cache_.contains(key)) {
                //Not found calculate it

                BDD f;
                const auto& input_tags = tag->input_tags();
                if(input_tags.empty()) {
                    //std::cout << "Node " << tag.launch_node() << " Base";
                    f = generate_pi_switch_func(tag->launch_node(), tag->trans_type());
                    //std::cout << " xfunc: " << f << "\n";
                } else {
                    //Recursive case
                    f = g_cudd.bddZero();
                    //int scenario_cnt = 0;

                    for(const auto& transition_scenario : input_tags) {
                        BDD f_scenario = g_cudd.bddOne();

                        for(const auto src_tag : transition_scenario) {
                            f_scenario &= this->build_bdd_xfunc(src_tag, level+1);
                        }

                        f |= f_scenario;
                        //std::cout << "Node " << node_id << " Scenario: " << scenario_cnt;
                        //std::cout << " xfunc: " << f << "\n";
                        //scenario_cnt++;
                    }
                }

                //Calulcated it, save it
                bdd_cache_.insert(key, f);

#ifdef BDD_CALC_DEBUG
                std::cout << tab << "Calculated BDD for " << node_id << " " << tag << " " << this->tg_.node_type(node_id) << " #SAT: " << bdd_sharpsat(f) << " " << f << "\n";
#endif

                return f;
            } else {
                BDD f = bdd_cache_.value(key);
#ifdef BDD_CALC_DEBUG
                std::cout << tab << "Looked up  BDD for " << node_id << " " << tag << " " << this->tg_.node_type(node_id) << " #SAT: " << bdd_sharpsat(f) << "\n";
#endif
                //Found it
                return f;
            }
            assert(0);
        }

    protected:

        double bdd_sharpsat_fraction(BDD f) {
            double dbl_count_custom_frac = CountMintermFraction(f.getNode());
            return dbl_count_custom_frac;
        }

        BDD generate_pi_switch_func(NodeId node_id, TransitionType trans) {

            //A constant generator
            if(const_gens_.count(node_id)) {
                //Constant generator always satisifes any input combination
                return g_cudd.bddOne();
            }

            //A generic input
            auto curr_iter = pi_curr_bdd_vars_.find(node_id);
            assert(curr_iter != pi_curr_bdd_vars_.end());

            auto next_iter = pi_next_bdd_vars_.find(node_id);
            assert(next_iter != pi_next_bdd_vars_.end());

            BDD f_curr = curr_iter->second;
            BDD f_next = next_iter->second;

            BDD switch_func;
            switch(trans) {
                case TransitionType::RISE:
                    switch_func = (!f_curr) & f_next; 
                    break;
                case TransitionType::FALL:
                    switch_func = f_curr & (!f_next); 
                    break;
                case TransitionType::HIGH:
                    switch_func = f_curr & f_next; 
                    break;
                case TransitionType::LOW:
                    switch_func = (!f_curr) & (!f_next); 
                    break;
                /*
                 *case TransitionType::STEADY:
                 *    switch_func = !(f_curr ^ f_next);
                 *    break;
                 *case TransitionType::SWITCH:
                 *    switch_func = f_curr ^ f_next;
                 *    break;
                 */
                default:
                    assert(0);
            }
            return switch_func;
        }

    protected:
        //BDD variable information
        std::unordered_map<NodeId,BDD> pi_curr_bdd_vars_;
        std::unordered_map<NodeId,BDD> pi_next_bdd_vars_;
        std::unordered_set<NodeId> const_gens_;

        BddCache bdd_cache_;
};

