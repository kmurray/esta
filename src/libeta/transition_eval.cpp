#include <cassert>
#include <vector>
#include <algorithm>

#include "cuddObj.hh"

#include "ExtTimingTag.hpp"
#include "transition_eval.hpp"

TransitionType evaluate_output_transition(const std::vector<TransitionType>& input_transitions, BDD f) {
    if(f.IsOne()) {
        return TransitionType::HIGH;
    } else if (f.IsZero()) {
        return TransitionType::LOW;
    }

    auto support_indicies = f.SupportIndices();
    assert(support_indicies.size() > 0);
    assert(input_transitions.size() == support_indicies.size());

    bool only_static_inputs_applied = false;
    for(size_t i = 0; i < support_indicies.size(); i++) {
        //It is entirely possible that the support of a function may be
        //less than the number of inputs (e.g. a useless input to a
        //logic function).
        //As a result we only fill in the variables which are explicitly
        //listed in the support
        size_t var_idx = support_indicies[i];

        BDD var = g_cudd.bddVar(var_idx);

        //FALL/LOW transitions result in logically false values, so we need to invert
        //the raw variable (which is non-inverted)
        if(input_transitions[i] == TransitionType::LOW || input_transitions[i] == TransitionType::FALL) {
            //Invert
            var = !var;
        }

        if(input_transitions[i] == TransitionType::RISE || input_transitions[i] == TransitionType::FALL) {
            only_static_inputs_applied = false;
        }

        f = f.Restrict(var);
    }

    //At this stage the logic function must have been fully determined
    assert(f.IsOne() || f.IsZero());

    //We now infer from the restricted logic function what the output transition from this node is
    //
    //If only static (i.e. High/Low) inputs were applied we generate a static High/Low output
    //otherwise we produced a dynamic transition (i.e. Rise/Fall)
    if(f.IsOne()) {
        if(only_static_inputs_applied) {
            return TransitionType::HIGH; 
        } else {
            return TransitionType::RISE; 
        }
    } else {
        assert(f.IsZero());
        
        if(only_static_inputs_applied) {
            return TransitionType::LOW; 
        } else {
            return TransitionType::FALL; 
        }
    }
}

TransitionType evaluate_output_transition(const std::vector<const ExtTimingTag*>& input_tags_scenario, BDD f) {
    std::vector<TransitionType> input_transitions;
    for(auto tag : input_tags_scenario) {
        input_transitions.push_back(tag->trans_type());
    }

    return evaluate_output_transition(input_transitions, f);
}
