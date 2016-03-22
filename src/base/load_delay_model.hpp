#pragma once
#include <map>
#include <string>
#include <tuple>
#include <iosfwd>
#include "TransitionType.hpp"

using CellDelayKey = std::tuple<
                         std::string, //Input pin name
                         std::string, //Output pin name
                         std::string, //Input pin transition
                         std::string //Output pin transition
                     >;

using CellDelayModel = std::map<
                        CellDelayKey,
                        float //Delay
                       >;

using DelayModel = std::map<
                       std::string, //cell name
                       CellDelayModel
                   >;

DelayModel load_delay_model(const std::string& filename);
DelayModel load_delay_model(std::istream& filename);