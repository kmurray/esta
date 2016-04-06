#pragma once
#include <vector>

enum class TransitionType {
    RISE,
    FALL,
    HIGH,
    LOW,
    CLOCK,
    //STEADY,
    //SWITCH
    UNKOWN
};

std::ostream& operator<<(std::ostream& os, const TransitionType& trans);
//bool operator<(const TransitionType& lhs, const TransitionType& rhs);

bool operator<(const std::vector<TransitionType>& lhs, const std::vector<TransitionType>& rhs);

