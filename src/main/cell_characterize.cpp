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

    assert(num_inputs == (size_t) support_size || f.IsOne() || f.IsZero());

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
    } else if (support_size == 1) {
        active_input_output_transitions = std::vector<std::set<std::tuple<TransitionType,TransitionType>>>(support_size);
        for(auto trans : valid_transitions) {
            active_input_output_transitions[0].insert(std::make_tuple(trans, trans));
        }
    } else {
        //Determine all possible input scenarios
        auto all_input_scenarios = cartesian_product(valid_transitions, support_size);

        //Evaluate all input -> output transitions scenarios,
        // constructing the set of active transitions as we go
        active_input_output_transitions = std::vector<std::set<std::tuple<TransitionType,TransitionType>>>(support_size);
        for(auto input_scenario : all_input_scenarios) {
            for(auto output_trans : valid_transitions) {

                std::cout << "{";
                for(size_t i = 0; i < input_scenario.size(); ++i) {
                    std::cout << input_scenario[i];
                    if(i != input_scenario.size() -1) { 
                        std::cout << ",";
                    }
                    active_input_output_transitions[i].insert(std::make_tuple(input_scenario[i], output_trans));
                }
                std::cout << "} -> " << output_trans << std::endl;
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

