#include <cassert>
#include <iostream>
#include <string>
#include "blif_data.hpp"
#include "blif_parse_common.hpp"

std::ostream& operator<<(std::ostream& os, const LogicValue& val) {
    switch(val) {
        case LogicValue::TRUE:
            os << "1";
            break;
        case LogicValue::FALSE:
            os << "0";
            break;
        case LogicValue::DC:
            os << "-";
            break;
        case LogicValue::UNKOWN:
            os << "X";
            break;
        default:
            assert(0);
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const LatchType& val) {
    switch(val) {
        case LatchType::FALLING_EDGE:
            os << "fe";
            break;
        case LatchType::RISING_EDGE:
            os << "re";
            break;
        case LatchType::ACTIVE_LOW:
            os << "al";
            break;
        case LatchType::ACTIVE_HIGH:
            os << "ah";
            break;
        case LatchType::ASYNCHRONOUS:
            os << "as";
            break;
        default:
            assert(0);
    }
    return os;
}

