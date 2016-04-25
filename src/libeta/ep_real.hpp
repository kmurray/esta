#pragma once
#include <boost/multiprecision/cpp_bin_float.hpp> 
#include "cudd.h"

//Forward decl
struct EpDoubleStruct;
typedef struct EpDoubleStruct EpDouble;

typedef boost::multiprecision::cpp_bin_float_50 real_t;

real_t EpDouble2Real(EpDouble& val);
real_t ApaInt2Real(DdApaNumber number, int digits);
