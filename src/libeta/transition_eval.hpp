#pragma once
#include "TransitionType.hpp"

class BDD;
class ExtTimingTag;

TransitionType evaluate_output_transition(const std::vector<TransitionType>& input_transitions, BDD f);

TransitionType evaluate_output_transition(const std::vector<const ExtTimingTag*>& input_tags_scenario, BDD f);
