#pragma once
#include <vector>
#include <unordered_set>
#include "ExtTimingTag.hpp"

class NullTransitionFilter {
    std::unordered_set<const ExtTimingTag*> identify_filtered_tags(std::vector<const ExtTimingTag*> input_tags, BDD node_func) {
        return std::unordered_set<const ExtTimingTag*>();
    }
};

class NextStateTransitionFilter {
    public :
        std::unordered_set<const ExtTimingTag*> identify_filtered_tags(std::vector<const ExtTimingTag*> input_tags, BDD node_func) {
            //Make a copy of the input tags and sort them by ascending arrival time
            std::vector<size_t> sorted_input_tag_idxs;;
            for(size_t i = 0; i < input_tags.size(); ++i) {
                sorted_input_tag_idxs.push_back(i);
            }
            assert(sorted_input_tag_idxs.size() == input_tags.size());


            auto sort_order = [&](size_t i, size_t j) {
                return input_tags[i]->arr_time() < input_tags[j]->arr_time();
            };
            std::sort(sorted_input_tag_idxs.begin(), sorted_input_tag_idxs.end(), sort_order);

            //Used to track those tags which are known to have arrived at the current
            //node.  We store the known transitions as they occur
            std::vector<TransitionType> input_transitions(input_tags.size(), TransitionType::UNKOWN);

            //Those tags which are filtered (i.e. have no effect on output timing)
            std::unordered_set<const ExtTimingTag*> filtered_tags;
            
            //Walk through the tags in arrival order, determining if the current
            //input will be filtered based on the known stable inputs
            for(auto i : sorted_input_tag_idxs) {
            
                if(input_is_filtered(i, input_transitions, node_func)) {
                    filtered_tags.insert(input_tags[i]);
                }

                //Keep track of the tags which have arrived (and could filter later arrivals)
                input_transitions[i] = input_tags[i]->trans_type();
            }

            return filtered_tags;
        }
    private:

        bool input_is_filtered(size_t input_idx, const std::vector<TransitionType>& input_transitions, BDD f) {
            //To figure out if the arriving_tag is filtered we need to look at the
            //logic function (node_func) and the transitions of any previously arrived
            //tags (arrived_tags).
            //
            //A tag is filtered if it is dominated by some other (arrived) input to
            //the logic function.
            //
            //We can identify a dominated input by looking at the logic function's
            //positive and negative Shannon co-factors for that variable. 
            //If the co-factors are equivalent, then the input has no impact on the
            //logic function output and is said to be dominated by other inputs (i.e.
            //filtered from the output).
            //
            //We can check for equivalent co-factors using BDDs. Since BDDs are constructed 
            //using Shannon co-factors we have already calculated the co-factors, and can
            //check if they are equivalent by XORing the co-factors together.
            //
            //In otherwords:
            //   Let f1 and f0 be the positive and negative the cofactors of f for a variable i
            //
            //      Then i is dominated if f0 == f1

            assert(input_transitions[input_idx] == TransitionType::UNKOWN); //Don't yet know this variable

            //Evaluate the node function for all the known input parameters
            // by restricting each input variable based on its final value
            for(size_t i = 0; i < input_transitions.size(); ++i) {
                if(input_transitions[i] == TransitionType::UNKOWN) continue; //Input unspecified

                //We store the node functions using variables 0..num_inputs-1
                //So get the resulting variable
                BDD var = g_cudd.bddVar(i);

                //FALL/LOW transitions result in logically false values in the next cycle, so we need to invert
                //the variable
                if(input_transitions[i] == TransitionType::LOW || input_transitions[i] == TransitionType::FALL) {
                    //Invert
                    var = !var;
                }

                //Refine f with this variable restriction
                f = f.Restrict(var);
            }

            //Get the co-factors of the restricted function
            BDD input_var = g_cudd.bddVar(input_idx);
            BDD f0 = f.Restrict(!input_var);
            BDD f1 = f.Restrict(input_var);

            //Check if the input variable has any possible 
            //impact on the logic functions final value
            return f0 == f1;
        }
};
