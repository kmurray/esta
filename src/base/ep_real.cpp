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

real_t ApaInt2Real(DdApaNumber value, int digits) {
    stringstream ss;
    
    for(int  i = 0; i < digits; ++i) {
        ss << value[i];
    }
    std::cout << ss.str() << std::endl;

    real_t converted_val;
    ss >> converted_val;

    assert(!ss.fail());

    return converted_val;
}
