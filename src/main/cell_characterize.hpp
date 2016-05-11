#pragma once
#include <vector>
#include <tuple>
#include <set>

#include "bdd.hpp"
#include "TransitionType.hpp"

std::vector<std::set<std::tuple<TransitionType,TransitionType>>> identify_active_transition_arcs(BDD f, size_t num_inputs);
