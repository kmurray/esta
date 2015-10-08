#pragma once
#include <vector>
#include <string>
#include <cstdarg>

#include "BlifParser.hpp"
#include "blif_data.hpp"

void throw_parse_error(BlifParser* context, const std::string& msg);
