#include <cassert>
#include <iostream>
#include <algorithm>

#include "cell_characterize.hpp"
#include "transition_eval.hpp"

std::vector<std::vector<TransitionType>> cartesian_product(const std::vector<TransitionType>& set, size_t n);

std::vector<std::set<std::tuple<TransitionType,TransitionType>>> identify_active_transition_arcs(BDD f, size_t num_inputs) {
    auto support_size = f.SupportSize(); 

    std::vector<TransitionType> valid_transitions = {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW};

    std::vector<std::set<std::tuple<TransitionType,TransitionType>>> active_input_output_transitions;

    //assert(num_inputs == (size_t) support_size || f.IsOne() || f.IsZero());

    if(f.IsOne() || f.IsZero()) {
        //Special case for constant nodes
        //TODO: Think more about this... this represents a constant generator, it shouldn't really be treated in the timing graph...
        active_input_output_transitions = std::vector<std::set<std::tuple<TransitionType,TransitionType>>>(num_inputs);

        for(size_t i = 0; i < num_inputs; ++i) {
            for(auto trans : valid_transitions) {
                if(f.IsZero()) {
                    active_input_output_transitions[i].insert(std::make_tuple(trans, TransitionType::LOW));
                } else {
                    assert(f.IsOne());
                    active_input_output_transitions[i].insert(std::make_tuple(trans, TransitionType::HIGH));
                }
            }
        }
    } else {
        //Determine all possible input scenarios
        auto all_input_scenarios = cartesian_product(valid_transitions, num_inputs);

        //Evaluate all input -> output transitions scenarios,
        // constructing the set of active transitions as we go
        active_input_output_transitions = std::vector<std::set<std::tuple<TransitionType,TransitionType>>>(support_size);
        for(auto input_scenario : all_input_scenarios) {
            TransitionType computed_output_trans = evaluate_output_transition(input_scenario, f);

            std::vector<TransitionType> possible_output_transitions;
            if(computed_output_trans == TransitionType::RISE || computed_output_trans == TransitionType::HIGH) {
                possible_output_transitions = {TransitionType::RISE, TransitionType::HIGH};
            } else {
                assert(computed_output_trans == TransitionType::FALL || computed_output_trans == TransitionType::LOW);
                possible_output_transitions = {TransitionType::FALL, TransitionType::LOW};
            }

            for(auto output_trans : possible_output_transitions) {
                //std::cout << "{";
                for(size_t i = 0; i < input_scenario.size(); ++i) {
                    //std::cout << input_scenario[i];
                    //if(i != input_scenario.size() -1) { 
                        //std::cout << ",";
                    //}
                    active_input_output_transitions[i].insert(std::make_tuple(input_scenario[i], output_trans));
                }
                //std::cout << "} -> " << output_trans << std::endl;
            }
        }
    }

    return active_input_output_transitions;
}

std::vector<std::vector<TransitionType>> cartesian_product(const std::vector<TransitionType>& set, size_t n) {
    std::vector<std::vector<TransitionType>> product;

    //Base case
    if(n == 1) {
        for(auto val : set) {
            product.push_back({val});
        }
        return product;
    } else {

        //Recurse
        std::vector<std::vector<TransitionType>> sub_product = cartesian_product(set, n-1);


        for(auto val : set) {
            for(auto& subset : sub_product) {
                product.push_back(subset);
                product[product.size()-1].push_back(val);
            }
        }
    }

    return product;
}

