#pragma once
#include <map>
#include <string>
#include <tuple>
#include <iosfwd>
#include <memory>
#include "TransitionType.hpp"
#include "PreCalcTransDelayCalc.hpp"

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

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(DelayModel& delay_model, const TimingGraph& tg);

TransitionType delay_model_trans_str_to_type(const std::string& trans_str);
extern const std::vector<std::string> delay_model_transitions;
