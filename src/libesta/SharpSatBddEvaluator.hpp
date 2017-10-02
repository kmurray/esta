#pragma once
#include <memory>
#include <random>
#include <iostream>

#include "SharpSatEvaluator.hpp"
#include "CuddSharpSatFraction.h"

#define USE_BDD_CACHE

//#define BDD_CALC_DEBUG
//#define DEBUG_PRINT_MINTERMS

enum class ConditionFunctionType {
    UNIFORM,
    NON_UNIFORM_ROUND_ROBIN,
    NON_UNIFORM_GROUPED_BY_BINARY_MINTERM,
    NON_UNIFORM_GROUPED_BY_GRAY_MINTERM
};

template<class Analyzer>
class SharpSatBddEvaluator : public SharpSatEvaluator<Analyzer> {
    private:
        typedef ObjectCacheMap<ExtTimingTag::cptr,BDD> BddCache;
    public:
        SharpSatBddEvaluator(const TimingGraph& tg, ConditionFunctionType cond_func_type, size_t nvars_per_input, std::shared_ptr<Analyzer> analyzer)
            : SharpSatEvaluator<Analyzer>(tg, analyzer)
            , nvars_per_input_(nvars_per_input)
            , cond_func_type_(cond_func_type) {

            if (cond_func_type == ConditionFunctionType::UNIFORM) {
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
            } else if (cond_func_type == ConditionFunctionType::NON_UNIFORM_ROUND_ROBIN || cond_func_type == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_BINARY_MINTERM || cond_func_type == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_GRAY_MINTERM) {
                //Collect primary inputs and identify constant generators
                std::vector<NodeId> primary_inputs;
                for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
                    auto node_type = tg.node_type(node_id);
                    if(node_type == TN_Type::INPAD_SOURCE || node_type == TN_Type::FF_SOURCE) {
                        primary_inputs.push_back(node_id);
                    } else if(node_type == TN_Type::CONSTANT_GEN_SOURCE) {
                        const_gens_.insert(node_id);
                    }
                }

                auto rng = std::default_random_engine();
                size_t num_minterms = 1 << nvars_per_input_;

                for (NodeId pi_node : primary_inputs) {
                    //Create the associated BDD vars
                    for (size_t ivar = 0; ivar < nvars_per_input_; ++ivar) {
                        pi_bdd_vars_[pi_node].push_back(g_cudd.bddVar());
                        g_cudd.pushVariableName("n" + std::to_string(pi_node) + "_" + std::to_string(ivar));
                    }

                    //Randomly assign numbers of minterms
                    size_t free_minterms = num_minterms;
                    for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH}) {
                        auto rand_minterms = std::uniform_int_distribution<size_t>(0, free_minterms);

                        size_t minterm_cnt = rand_minterms(rng);

                        assigned_minterm_counts_[pi_node][trans] = minterm_cnt;

                        //Decrement the free minterms by those assigned to the current transition
                        free_minterms -= minterm_cnt;
                        assert(free_minterms >= 0);
                    }
                    //Allocate any remaining minterms to the LOW transition
                    assigned_minterm_counts_[pi_node][TransitionType::LOW] = free_minterms;

                    //All minterms must be assgined
                    size_t assigned_minterm_cnt = 0;
                    std::cout << "Node " << pi_node << " Cond Func Minterms: " << std::endl;
                    for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                        auto trans_minterm_cnt = assigned_minterm_counts_[pi_node][trans];
                        assigned_minterm_cnt += trans_minterm_cnt;

                        std::cout << "\t" << trans << ": " << trans_minterm_cnt << std::endl;

                    }
                    std::cout << "\t" << "total minterms: " << assigned_minterm_cnt << std::endl;
                    assert(assigned_minterm_cnt == num_minterms);

                    if (cond_func_type == ConditionFunctionType::NON_UNIFORM_ROUND_ROBIN) {
                        cond_funcs_[pi_node] = create_condition_functions_round_robin(pi_node, assigned_minterm_counts_[pi_node]);
                    } else if (cond_func_type == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_GRAY_MINTERM) {
                        cond_funcs_[pi_node] = create_condition_functions_group_by_gray_minterm(pi_node, assigned_minterm_counts_[pi_node]);
                    } else {
                        assert(cond_func_type == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_BINARY_MINTERM);
                        cond_funcs_[pi_node] = create_condition_functions_group_by_binary_minterm(pi_node, assigned_minterm_counts_[pi_node]);
                    }

                    for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                        std::cout << trans << ": " << bdd_sharpsat_fraction(cond_funcs_[pi_node][trans]) << "\n";
                        cond_funcs_[pi_node][trans].PrintCover();
                    }
                }
            } else {
                assert(false && "invalid condition function type");
            }
            bdd_cache_ = BddCache(false);

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
            bdd_cache_ = BddCache(false);

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

            if (cond_func_type_ == ConditionFunctionType::UNIFORM) {

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
            } else if (cond_func_type_ == ConditionFunctionType::NON_UNIFORM_ROUND_ROBIN) {
                return cond_funcs_[node_id][trans];
            } else if (cond_func_type_ == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_BINARY_MINTERM) {
                return cond_funcs_[node_id][trans];
            } else {
                assert(cond_func_type_ == ConditionFunctionType::NON_UNIFORM_GROUPED_BY_GRAY_MINTERM && "Invalid cond func type");

                return cond_funcs_[node_id][trans];
            }
        }

        std::map<TransitionType,BDD> create_condition_functions_round_robin(NodeId node, std::map<TransitionType,size_t> minterm_counts) {
            std::map<TransitionType,BDD> cond_funcs;

            for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                cond_funcs[trans] = g_cudd.bddZero(); //Initially false
            }

            size_t iminterm = 0; //Current minterm index

            auto sum = [&](size_t prior, const std::pair<TransitionType,size_t> val) {
                return prior + val.second;
            };
            size_t current_minterm_count = std::accumulate(minterm_counts.begin(), minterm_counts.end(), 0u, sum);
            while(current_minterm_count > 0) {
                
                for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                    if (minterm_counts[trans] > 0) {
                        set_minterm(node, cond_funcs[trans], iminterm++);

                        --minterm_counts[trans];
                    }
                }
                current_minterm_count = std::accumulate(minterm_counts.begin(), minterm_counts.end(), 0u, sum);
            }

            size_t total_minterms = 1 << nvars_per_input_;
            assert (iminterm == total_minterms);

            return cond_funcs;
        }

        std::map<TransitionType,BDD> create_condition_functions_group_by_binary_minterm(NodeId node, std::map<TransitionType,size_t> minterm_counts) {
            std::map<TransitionType,BDD> cond_funcs;
            for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                cond_funcs[trans] = g_cudd.bddZero(); //Initially false
            }


            auto minterm_sort = [](const std::pair<TransitionType,size_t>& lhs, const std::pair<TransitionType,size_t>& rhs) {
                //Descending order
                return lhs.second >= rhs.second;
            };

            auto minterm_sum = [](size_t prior, const std::pair<TransitionType,size_t>& val) {
                return prior + val.second;
            };

            std::vector<std::pair<TransitionType,size_t>> sorted_minterm_counts;
            std::copy(minterm_counts.begin(), minterm_counts.end(), std::back_inserter(sorted_minterm_counts));
            std::sort(sorted_minterm_counts.begin(), sorted_minterm_counts.end(), minterm_sort);

            for (auto val : sorted_minterm_counts) {
                std::cout << val.first << ": " << val.second << " (" << std::log2(val.second) << ")\n";
            }


            size_t iminterm = 0;

            size_t unassigned_minterm_count = std::accumulate(sorted_minterm_counts.begin(),
                                                              sorted_minterm_counts.end(),
                                                              0u,
                                                              minterm_sum);
            while (unassigned_minterm_count > 0) {
                for (auto& val : sorted_minterm_counts) {
                    auto& trans = val.first;
                    auto& num_minterms = val.second;

                    if (num_minterms == 0) continue;

                    size_t log2_minterms = std::floor(std::log2(num_minterms));

                    size_t num_minterms_to_add = 1 << log2_minterms;

                    size_t iminterm_end = iminterm + num_minterms_to_add;
                    for(; iminterm < iminterm_end; ++iminterm) {
                        set_minterm(node, cond_funcs[trans], iminterm);
                    }

                    num_minterms -= num_minterms_to_add;
                }
                unassigned_minterm_count = std::accumulate(sorted_minterm_counts.begin(),
                                                           sorted_minterm_counts.end(),
                                                           0u,
                                                           minterm_sum);
            }

            size_t total_minterms = 1 << nvars_per_input_;
            assert(iminterm == total_minterms);


            return cond_funcs;
        }

        std::map<TransitionType,BDD> create_condition_functions_group_by_gray_minterm(NodeId node, std::map<TransitionType,size_t> minterm_counts) {
            std::map<TransitionType,BDD> cond_funcs;
            for (auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                cond_funcs[trans] = g_cudd.bddZero(); //Initially false
            }


            auto minterm_sort = [](const std::pair<TransitionType,size_t>& lhs, const std::pair<TransitionType,size_t>& rhs) {
                //Descending order
                return lhs.second >= rhs.second;
            };

            auto minterm_sum = [](size_t prior, const std::pair<TransitionType,size_t>& val) {
                return prior + val.second;
            };

            std::vector<std::pair<TransitionType,size_t>> sorted_minterm_counts;
            std::copy(minterm_counts.begin(), minterm_counts.end(), std::back_inserter(sorted_minterm_counts));
            std::sort(sorted_minterm_counts.begin(), sorted_minterm_counts.end(), minterm_sort);

            for (auto val : sorted_minterm_counts) {
                std::cout << val.first << ": " << val.second << " (" << std::log2(val.second) << ")\n";
            }


            size_t iminterm = 0;

            size_t unassigned_minterm_count = std::accumulate(sorted_minterm_counts.begin(),
                                                              sorted_minterm_counts.end(),
                                                              0u,
                                                              minterm_sum);
            while (unassigned_minterm_count > 0) {
                for (auto& val : sorted_minterm_counts) {
                    auto& trans = val.first;
                    auto& num_minterms = val.second;

                    if (num_minterms == 0) continue;

                    size_t log2_minterms = std::floor(std::log2(num_minterms));

                    size_t num_minterms_to_add = 1 << log2_minterms;

                    size_t iminterm_end = iminterm + num_minterms_to_add;
                    for(; iminterm < iminterm_end; ++iminterm) {
                        //Note that we count the minterms (iminterm) in standard binary,
                        //but assign them based on their 'gray code' value; this should result
                        //in simpler functions since assigned minterms will be adjacent in 
                        //binary space (c.f. Karnaugh map) and easily covered
                        set_minterm(node, cond_funcs[trans], binary_to_gray(iminterm));
                    }

                    num_minterms -= num_minterms_to_add;
                }
                unassigned_minterm_count = std::accumulate(sorted_minterm_counts.begin(),
                                                           sorted_minterm_counts.end(),
                                                           0u,
                                                           minterm_sum);
            }

            size_t total_minterms = 1 << nvars_per_input_;
            assert(iminterm == total_minterms);


            return cond_funcs;
        }

        void set_minterm(NodeId node, BDD& f, size_t iminterm) {

            BDD minterm = g_cudd.bddOne();
            for (size_t ivar = 0; ivar < nvars_per_input_; ++ivar) {
                //Build the minterm function by inspecting the bit pattern of iminterm
                size_t var_mask = (1u << ivar);

                auto masked_value = var_mask & iminterm;
                if (masked_value != 0) {
                    minterm &= pi_bdd_vars_[node][ivar];
                } else {
                    minterm &= !pi_bdd_vars_[node][ivar];
                }
            }
            f |= minterm;

            //minterm.PrintCover();
            //std::cout << "minterm: " << minterm << "\n";
            //std::cout << "f: " << f << "\n";
        }

        size_t binary_to_gray(size_t binary_value) {
            //See: https://en.wikipedia.org/wiki/Gray_code
            return binary_value ^ (binary_value >> 1);
        }
    protected:
        size_t nvars_per_input_;

        //BDD variable information
        std::unordered_map<NodeId,BDD> pi_curr_bdd_vars_;
        std::unordered_map<NodeId,BDD> pi_next_bdd_vars_;
        std::unordered_set<NodeId> const_gens_;
        std::map<NodeId,std::vector<BDD>> pi_bdd_vars_;

        std::map<NodeId,std::map<TransitionType,size_t>> assigned_minterm_counts_;
        ConditionFunctionType  cond_func_type_;
        std::map<NodeId,std::map<TransitionType,BDD>> cond_funcs_;

        BddCache bdd_cache_;
};
