#pragma once

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

