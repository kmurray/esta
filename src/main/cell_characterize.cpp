#include <cassert>
#include <algorithm>

#include "cell_characterize.hpp"

std::vector<std::vector<TransitionType>> cartesian_product(const std::vector<TransitionType>& set, size_t n);
TransitionType evaluate_output_transition(std::vector<TransitionType> input_transitions, BDD f);

std::vector<std::set<std::tuple<TransitionType,TransitionType>>> identify_active_transition_arcs(BDD f) {
    auto support_size = f.SupportSize(); 

    std::vector<TransitionType> valid_transitions = {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW};

    //Determine all possible input scenarios
    auto all_input_scenarios = cartesian_product(valid_transitions, support_size);

    //Evaluate all input -> output transitions scenarios,
    // constructing the set of active transitions as we go
    std::vector<std::set<std::tuple<TransitionType,TransitionType>>> active_input_output_transitions(support_size);
    for(auto input_scenario : all_input_scenarios) {
        auto output_trans = evaluate_output_transition(input_scenario, f);

        for(size_t i = 0; i < input_scenario.size(); ++i) {
            active_input_output_transitions[i].insert(std::make_tuple(input_scenario[i], output_trans));
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

TransitionType evaluate_output_transition(std::vector<TransitionType> input_transitions, BDD f) {
    if(f.IsOne()) {
        return TransitionType::HIGH;
    } else if (f.IsZero()) {
        return TransitionType::LOW;
    } else {
        //CUDD assumes we provide the inputs defined up to the maximum variable index in the bdd
        //so we need to allocate a vector atleast as large as the highest index.
        //To do this we query the support indicies and take the max + 1 of these as the size.
        //The vectors and then initialized (all values false) to this size.
        //Note that cudd will only read the indicies associated with the variables in the bdd, 
        //so the values of variablse not in the bdd don't matter

        auto support_indicies = f.SupportIndices();
        assert(support_indicies.size() > 0);
        size_t max_var_index = *std::max_element(support_indicies.begin(), support_indicies.end());
        std::vector<int> initial_inputs(max_var_index+1, 0);
        std::vector<int> final_inputs(max_var_index+1, 0);

        for(size_t i = 0; i < support_indicies.size(); i++) {
            //It is entirely possible that the support of a function may be
            //less than the number of inputs (e.g. a useless input to a
            //logic function).
            //As a result we only fill in the variables which are explicitly
            //listed in the support
            size_t var_idx = support_indicies[i];

            //We do expect the node inputs to be a superset of
            //the support
            assert(var_idx < input_transitions.size());
            switch(input_transitions[i]) {
                case TransitionType::RISE:
                    initial_inputs[var_idx] = 0;
                    final_inputs[var_idx] = 1;
                    break;
                case TransitionType::FALL:
                    initial_inputs[var_idx] = 1;
                    final_inputs[var_idx] = 0;
                    break;
                case TransitionType::HIGH:
                    initial_inputs[var_idx] = 1;
                    final_inputs[var_idx] = 1;
                    break;
                case TransitionType::LOW:
                    initial_inputs[var_idx] = 0;
                    final_inputs[var_idx] = 0;
                    break;

                default:
                    assert(0);
            }
        }

        BDD init_output = f.Eval(initial_inputs.data());
        BDD final_output = f.Eval(final_inputs.data());

        if(init_output.IsOne()) {
            if(final_output.IsOne()) {
                return TransitionType::HIGH;
            } else {
                assert(final_output.IsZero());
                return TransitionType::FALL;
            }
        } else {
            assert(init_output.IsZero());
            if(final_output.IsOne()) {
                return TransitionType::RISE;
            } else {
                assert(final_output.IsZero());
                return TransitionType::LOW;
            }
        }
    }

    assert(0); //Shouldn't get here
}
