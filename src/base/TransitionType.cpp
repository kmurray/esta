#include <iostream>
#include <cassert>

#include "TransitionType.hpp"

std::ostream& operator<<(std::ostream& os, const TransitionType& trans) {
    if(trans == TransitionType::RISE) os << "RISE";
    else if (trans == TransitionType::FALL) os << "FALL";
    else if (trans == TransitionType::HIGH) os << "HIGH";
    else if (trans == TransitionType::LOW) os << "LOW";
    else if (trans == TransitionType::CLOCK) os << "CLOCK";
    else if (trans == TransitionType::UNKOWN) os << "UNKOWN";
    else assert(0);
    return os;
}

