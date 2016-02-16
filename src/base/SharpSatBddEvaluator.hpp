#pragma once
#include "ep_real.hpp"
#include "SharpSatEvaluator.hpp"

//#define BDD_CALC_DEBUG


template<class Analyzer>
class SharpSatBddEvaluator : public SharpSatEvaluator<Analyzer> {
    public:
        typedef typename SharpSatBddEvaluator<Analyzer>::count_support count_support;

        SharpSatBddEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer, int nvars, int approx_threshold, float node_ratio, float quality)
            : SharpSatEvaluator<Analyzer>(tg, analyzer, nvars)
            , bdd_approx_threshold_(approx_threshold) 
            , bdd_approx_node_ratio_(node_ratio)
            , bdd_approx_quality_(quality) {
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
                }
            }
        }

        count_support count_sat(const ExtTimingTag& tag, NodeId node_id) override {
            BDD f = build_bdd_xfunc(tag, node_id, 0);

            EpDouble f_count_cudd;
            Cudd_EpdCountMinterm(f.manager(), f.getNode(), this->nvars_, &f_count_cudd);
            real_t f_count = EpDouble2Real(f_count_cudd);

            return {f_count, f.SupportIndices(), true};
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
            bdd_cache_.print_stats(); 
            bdd_cache_.clear(); 
            bdd_cache_.reset_stats(); 

            //Reset the default re-order size
            //Re-ordering really big BDDs is slow (re-order time appears to be quadratic in size)
            //So cap the size for re-ordering to something reasonable when re-starting
            /*
             *auto next_reorder = std::min(g_cudd.ReadNextReordering(), 100004u);
             *g_cudd.SetNextReordering(next_reorder);
             */

             g_cudd.SetNextReordering(4096);
        }

    protected:


        BDD build_bdd_xfunc(const ExtTimingTag& tag, const NodeId node_id, int level) {
            /*std::cout << "build_xfunc at Node: " << node_id << " TAG: " << tag << "\n";*/
            auto key = std::make_pair(node_id, tag.trans_type());

            //Indent based on recursion level 
            std::string tab(level, ' ');

#ifdef BDD_CALC_DEBUG
            std::cout << tab << "Requested BDD for " << node_id << " " << tag.trans_type() << " " << this->tg_.node_type(node_id) << "\n";
#endif

            if(!bdd_cache_.contains(key)) {
                //Not found calculate it

                BDD f;
                auto node_type = this->tg_.node_type(node_id);
                if(node_type == TN_Type::INPAD_SOURCE || node_type == TN_Type::FF_SOURCE) {
                    /*std::cout << "Node " << node_id << " Base";*/
                    f = generate_pi_switch_func(node_id, tag.trans_type());
                    /*std::cout << " xfunc: " << f << "\n";*/
                } else {
                    f = g_cudd.bddZero();
                    int scenario_cnt = 0;
                    //Uniquify transitions
                    auto input_transitions = tag.input_transitions();

                    //Uniquify the transitions
                    /*
                     *std::sort(input_transitions.begin(), input_transitions.end());
                     *auto uniq_iter = std::unique(input_transitions.begin(), input_transitions.end());
                     *input_transitions.resize(std::distance(input_transitions.begin(), uniq_iter));
                     */

                    for(const auto& transition_scenario : input_transitions) {
                        BDD f_scenario = g_cudd.bddOne();
                        
                        //We need to keep a separate transition index since some edges
                        //(e.g. clock edges) do not count towards input transitions
                        int trans_idx = 0;

                        for(int edge_idx = 0; edge_idx < this->tg_.num_node_in_edges(node_id); edge_idx++) {
                            EdgeId edge_id = this->tg_.node_in_edge(node_id, edge_idx);
                            NodeId src_node_id = this->tg_.edge_src_node(edge_id);

                            if(this->tg_.node_type(src_node_id) == TN_Type::FF_CLOCK) {
                                continue;
                            }

                            assert(trans_idx < (int) transition_scenario.size());
                            
                            auto& src_tags = this->analyzer_->setup_data_tags(src_node_id);
                            for(auto& src_tag: src_tags) {
                                if(src_tag.trans_type() == transition_scenario[edge_idx]) {
                                    //Found the matching tag
                                    f_scenario &= this->build_bdd_xfunc(src_tag, src_node_id, level+1);
                                    break;
                                }
                            }
                            trans_idx++;
                        }

                        f |= f_scenario;
                        /*std::cout << "Node " << node_id << " Scenario: " << scenario_cnt;*/
                        /*std::cout << " xfunc: " << f << "\n";*/

                        scenario_cnt++;
                    }
                }

                //Calulcated it, save it
                bdd_cache_.insert(key, f);

#ifdef BDD_CALC_DEBUG
                std::cout << tab << "Calculated BDD for " << node_id << " " << tag.trans_type() << " " << this->tg_.node_type(node_id) << " " << f << "\n";
#endif

                return f;
            } else {
#ifdef BDD_CALC_DEBUG
                std::cout << tab << "Looked up  BDD for " << node_id << " " << tag.trans_type() << " " << this->tg_.node_type(node_id) << "\n";
#endif
                //Found it
                return bdd_cache_.value(key);
            }
            assert(0);
        }

        BDD generate_pi_switch_func(NodeId node_id, TransitionType trans) {
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

        int bdd_approx_threshold_;
        float bdd_approx_node_ratio_;
        float bdd_approx_quality_;

        ObjectCacheMap<std::pair<NodeId,TransitionType>,BDD> bdd_cache_;
};

