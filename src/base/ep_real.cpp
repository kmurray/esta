#include <sstream>
#include "epd.h" //From CUDD
#include "ep_real.hpp"

using std::stringstream;

real_t EpDouble2Real(EpDouble& val) {
    char str[100]; //Hope this is big enough...

    EpdGetString(&val, str);
    stringstream ss(str);
    real_t converted_val;
    ss >> converted_val;
    assert(!ss.fail());
    return converted_val;
}
