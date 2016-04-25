#pragma once
#include <map>
#include <queue>
#include <cmath>
#include "SharpSatBddEvaluator.hpp"
#include "TimingGraph.hpp"

template<class Analyzer>
class SharpSatDecompBddEvaluator : public SharpSatBddEvaluator<Analyzer> {

    public:
        SharpSatDecompBddEvaluator(const TimingGraph& tg, std::shared_ptr<Analyzer> analyzer, int nvars)
            : SharpSatBddEvaluator<Analyzer>(tg, analyzer, nvars)
            , node_input_dependancies_(tg.num_nodes()) {

            //Determine node Logical Input dependancies
            std::queue<NodeId> nodes;
            for(NodeId node_id : tg.logical_inputs()) {
                nodes.push(node_id); //Initialize the current traversal
                node_input_dependancies_[node_id].push_back(node_id); //Initialize dependancies
            }
            while(!nodes.empty()) {
                NodeId node_id = nodes.front();
                nodes.pop();

                //Save the depndancies at this node
                for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                    EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);
                    NodeId src_node_id = tg.edge_src_node(edge_id);

                    std::copy(node_input_dependancies_[src_node_id].begin(), node_input_dependancies_[src_node_id].end(), 
                              std::back_inserter(node_input_dependancies_[node_id]));
                }
                //Uniquify
                std::sort(node_input_dependancies_[node_id].begin(), node_input_dependancies_[node_id].end());
                auto iter = std::unique(node_input_dependancies_[node_id].begin(), node_input_dependancies_[node_id].end());
                node_input_dependancies_[node_id].resize(std::distance(node_input_dependancies_[node_id].begin(),iter)); 

                //DEBUG
#if 0
                std::cout << "Node " << node_id << " Deps: {";
                for(NodeId src_node_id : node_input_dependancies_[node_id]) {
                    std::cout << src_node_id << ", ";
                }
                std::cout << "}\n";
#endif

                //Add the output nodes to continue the traversal
                for(int edge_idx = 0; edge_idx < tg.num_node_out_edges(node_id); edge_idx++) {
                    EdgeId edge_id = tg.node_out_edge(node_id, edge_idx);
                    NodeId sink_node_id  = tg.edge_sink_node(edge_id);
                    nodes.push(sink_node_id);
                }
            }
        }

        typedef typename SharpSatDecompBddEvaluator<Analyzer>::count_support count_support;
        typedef typename SharpSatDecompBddEvaluator<Analyzer>::count_support::count_type count_type;

        count_support count_sat(const ExtTimingTag& tag, NodeId node_id) override;

    protected:
        count_support count_sat_recurr(const ExtTimingTag& tag, NodeId node_id);
        bool is_node_support_disjoint(NodeId node_id);
        
    protected:
        std::vector<std::vector<NodeId>> node_input_dependancies_; //The logical inputs that effect the specific node [0..tg_.num_nodes()][0..size]

        std::map<std::pair<NodeId,TransitionType>, count_support> count_table_;
};


#include <algorithm>

template<class Analyzer>
typename SharpSatDecompBddEvaluator<Analyzer>::count_support SharpSatDecompBddEvaluator<Analyzer>::count_sat(const ExtTimingTag& tag, NodeId node_id) {
    auto key = std::make_pair(node_id, tag.trans_type()); 
    auto iter = count_table_.find(key);
    if(iter == count_table_.end()) {
        //Haven't solved this subproblem yet -- solve it
        count_support sat_cnt_support = this->count_sat_recurr(tag, node_id);

        //Save the sub-problem result
        count_table_.insert({key, sat_cnt_support});

        std::cout << "Caclulated (" << node_id << ", " << tag.trans_type() << ") #SAT: " << sat_cnt_support.count << "\n";
        return sat_cnt_support;
    } else {
        //Found sub-problem in pre-calculated table
        std::cout << "Looked Up  (" << node_id << ", " << tag.trans_type() << ") #SAT: " << iter->second.count << "\n";
        return iter->second;
    }
}

template<class Analyzer>
typename SharpSatDecompBddEvaluator<Analyzer>::count_support SharpSatDecompBddEvaluator<Analyzer>::count_sat_recurr(const ExtTimingTag& tag, NodeId node_id) {
    //std::cout << "*Node: " << node_id << " Trans: " << tag.trans_type() << "\n";
    count_support sat_cnt_supp;
    sat_cnt_supp.count = std::numeric_limits<count_type>::quiet_NaN();

    auto node_type = this->tg_.node_type(node_id);
    if(node_type == TN_Type::INPAD_SOURCE || node_type == TN_Type::FF_SOURCE) {
        //Base case -- a primary input
        BDD f = this->generate_pi_switch_func(node_id, tag.trans_type());

        sat_cnt_supp.support = f.SupportIndices();
        assert(sat_cnt_supp.support.size() == 2);
        sat_cnt_supp.count = f.CountMinterm(sat_cnt_supp.support.size());
        sat_cnt_supp.pure_bdd = true;
        //std::cout << "*Node: " << node_id << " BASE Func: " << f << " BASE #SAT: " << sat_cnt << "\n"; 
    } else {
        //Recursive case
        //
        //The naive approach is to recursively build the BDD representing the switch function at node,
        //and then to perform #SAT on it directly.  
        //That is, we consider all sets of input transitions (scenarios) at this node which generate the
        //current tag's transition. The switch function of each input in a scenario are ANDed together
        //to form the scenario switch func.  The set of scenario switch funcs are then ORed together.
        //This approach is exact (in particular it handles re-convergence exactly), but can be 
        //quite expensive.
        //
        //As an optimization, we attempt to detect when the functions being ANDed/ORed have disjoint
        //supports.  If this occurs the structure can be exploited using the following #SAT identities:
        //
        //  Inversion:
        //      #(!fa) = 2**na - #(fa)          
        //
        //      The number of UNSAT assignments can be calculated directly from the size of the functions
        //      support (na) and the number of SAT assignments #(fa).
        //
        //  Disjoint AND:
        //      #(fa & fb) = #(fa) * #(fb) 
        //
        //      Here #(fa) and #(fb) are the number of SAT assignments over each functions support,
        //      and the supports of fa and fb are assumed to be disjoint.
        //
        //  Disjoin OR:
        //      #(fa | fb) = 2**(na)*#(fb) + 2**(nb)*#(fb) - #(fa)*#(fb)        #2 pow, 3 mul, 2 add
        //
        //      Derived directly using Inversion, DeMorgan's Law and Disjoint AND.
        //

        //Determine if the switch function is disjoint
        bool disjoint = is_node_support_disjoint(node_id);

        if(disjoint) {
            //Calculate the counts directly via identities
            
            //Calculate the counts for each scenario (set of input signal transitions)
            std::vector<count_support> scenario_sat_counts_support;
            for(const auto& transition_scenario : tag.input_transitions()) {
                //We need to keep a separate transition index since some edges
                //(e.g. clock edges) do not count towards input transitions
                int trans_idx = 0;

                std::vector<count_support> input_sat_counts_support;
                for(int edge_idx = 0; edge_idx < this->tg_.num_node_in_edges(node_id); edge_idx++) {
                    EdgeId edge_id = this->tg_.node_in_edge(node_id, edge_idx);
                    NodeId src_node_id = this->tg_.edge_src_node(edge_id);

                    if(this->tg_.node_type(src_node_id) == TN_Type::FF_CLOCK) {
                        continue;
                    }

                    assert(trans_idx < (int) transition_scenario.size());
                    
                    auto& src_tags = this->analyzer_->setup_data_tags(src_node_id);
                    auto pred = [&](const ExtTimingTag& src_tag) {
                        return src_tag.trans_type() == transition_scenario[edge_idx]; 
                    };
                    auto src_tag_iter = std::find_if(src_tags.begin(), src_tags.end(), pred);
                    assert(src_tag_iter != src_tags.end());
                    //Found the matching tag

                    //Calculate the SAT count
                    // NOTE: we call the non-reccur version here to take advantage of memoizing previous answers!
                    count_support scenario_cnt_support = this->count_sat(*src_tag_iter, src_node_id);
                    input_sat_counts_support.push_back(scenario_cnt_support);

                    //std::cout << "\t" << "*Node: " << node_id << " In Edge Idx: " << edge_idx << " #SAT: " << scenario_cnt << "\n";
                    trans_idx++;
                }

                //Merge the scenario counts by ANDing them together using the Disjiont AND #SAT identity.
                auto disjoint_AND_reduce = [](const count_support& a, const count_support& b) -> count_support {
                    count_support result;
                    result.count = a.count * b.count;

                    //Do the union
                    std::set_union(a.support.begin(), a.support.end(), 
                                   b.support.begin(), b.support.end(), 
                                   std::back_inserter(result.support));

                    //std::cout << "AND -> A: " << a << " B: " << b << " RESULT: " << result << "\n"; 

                    result.pure_bdd = false;
                    return result; 
                };
                count_support scenario_cnt_sup = std::accumulate(input_sat_counts_support.begin()+1, 
                                                                 input_sat_counts_support.end(), 
                                                                 *input_sat_counts_support.begin(), //Initial
                                                                 disjoint_AND_reduce);

                scenario_sat_counts_support.push_back(scenario_cnt_sup);
                //std::cout << "\t" << "*Node: " << node_id << " Scenario #SAT: " << scenario_cnt << "\n";
            }

            /*
             *if(node_id == 11) {
             *    std::cout << "FOUND";
             *}
             */
            
            //Merge the scenario counts by ORing them together using the Disjoint OR #SAT identity.
            auto disjoint_OR_reduce = [](const count_support& a, const count_support& b) -> count_support  {
                count_support result;

                result.count = pow(2,a.support.size())*b.count + pow(2,b.support.size())*a.count - a.count*b.count;

                //Do the union
                std::set_union(a.support.begin(), a.support.end(), 
                               b.support.begin(), b.support.end(), 
                               std::back_inserter(result.support));

                //std::cout << "OR  -> A: " << a << " B: " << b << " RESULT: " << result << "\n"; 
                result.pure_bdd = false;

                return result;
            };

            auto pessimistic_OR_reduce = [](const count_support& a, const count_support& b) -> count_support  {
                count_support result;

                std::vector<unsigned int> a_unique_support;
                std::vector<unsigned int> b_unique_support;

                std::set_difference(a.support.begin(), a.support.end(),
                                    b.support.begin(), b.support.end(),
                                    std::back_inserter(a_unique_support));
                std::set_difference(b.support.begin(), b.support.end(),
                                    a.support.begin(), a.support.end(),
                                    std::back_inserter(b_unique_support));

                result.count = a.count + b.count;

                //Do the union
                std::set_union(a.support.begin(), a.support.end(), 
                               b.support.begin(), b.support.end(), 
                               std::back_inserter(result.support));

                //std::cout << "OR  -> A: " << a << " B: " << b << " RESULT: " << result << "\n"; 
                result.pure_bdd = false;
                return result;
            };

            //std::cout << "Node: " << node_id << "\n";
            sat_cnt_supp = std::accumulate(scenario_sat_counts_support.begin()+1, 
                                           scenario_sat_counts_support.end(), 
                                           *scenario_sat_counts_support.begin(), //Initial
                                           pessimistic_OR_reduce);

            
            //std::cout << "\t" << "*Node: " << node_id << " #scenarios " << scenario_sat_counts.size() << " #SAT: " << sat_cnt << "\n";
        } else {
            assert(!disjoint);
            //Calculate the sat count via building a BDD
            //Since the switch functions are not disjoint, we need to build the BDD to correctly handle reconvergence
            BDD f = this->build_bdd_xfunc(tag, node_id);

            sat_cnt_supp.count = f.CountMinterm(this->nvars_);
            sat_cnt_supp.support = f.SupportIndices();
        }
    }
    //std::cout << "Node " << node_id << ": " << sat_cnt_supp << "\n";
    return sat_cnt_supp;
}


template<class Analyzer>
bool SharpSatDecompBddEvaluator<Analyzer>::is_node_support_disjoint(NodeId node_id) {
    std::map<NodeId,int> logical_input_dep_cnt;

    //TODO: do a more intelligent search for disjoint (e.g. amoung pairs)
    if(this->tg_.num_node_in_edges(node_id) > 1) {
        for(int edge_idx = 0; edge_idx < this->tg_.num_node_in_edges(node_id); edge_idx++) {
            EdgeId edge_id = this->tg_.node_in_edge(node_id, edge_idx);
            NodeId src_node_id = this->tg_.edge_src_node(edge_id);

            for(NodeId input_node_id : node_input_dependancies_[src_node_id]) {
                logical_input_dep_cnt[input_node_id]++;
            }

        }
        //std::cout << "Node " << node_id << " Dep Intersection: {";
        for(auto kv : logical_input_dep_cnt) {
            if(kv.second > 1) {
                //std::cout << kv.first << ", ";
                return false;
            }
        }
        //std::cout << "}\n";
    }
    return true;
}
