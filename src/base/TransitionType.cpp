#include <iostream>
#include <cassert>

#include "TransitionType.hpp"

std::ostream& operator<<(std::ostream& os, const TransitionType& trans) {
    if(trans == TransitionType::RISE) os << "R";
    else if (trans == TransitionType::FALL) os << "F";
    else if (trans == TransitionType::HIGH) os << "H";
    else if (trans == TransitionType::LOW) os << "L";
    else if (trans == TransitionType::CLOCK) os << "C";
    else if (trans == TransitionType::UNKOWN) os << "U";
    else assert(0);
    return os;
}

bool operator<(const std::vector<TransitionType>& lhs, const std::vector<TransitionType>& rhs) {
    for(size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if(lhs[i] < rhs[i]) return true;
    }
    return false;
    //return true;
}
