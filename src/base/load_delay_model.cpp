#include <iostream>
#include <iterator>
#include <fstream>
#include <vector>
#include <cassert>
#include <tuple>

#include <yaml-cpp/yaml.h>

#include "load_delay_model.hpp"

using std::get;

TransitionType trans_str_to_type(const std::string& trans_str);
const std::vector<std::string> valid_transitions = {"rise", "fall", "high", "low"};


DelayModel load_delay_model(const std::string& filename) {
    DelayModel delay_model;

    YAML::Node doc = YAML::LoadFile(filename);

    assert(doc.IsMap());

    const auto& cells = doc["cells"];

    assert(cells.IsSequence());

    //Each cell type
    for(const auto& cell_description : cells) {
        std::string cell_name = cell_description["cell_name"].as<std::string>();

        std::cout << "Cell: " << cell_name << "\n";

        const auto& cell_pins = cell_description["cell_pins"];
        assert(cell_pins.IsMap());

        std::vector<std::string> input_pins;
        for(const auto& pin : cell_pins["input"]) {
            input_pins.push_back(pin.as<std::string>());
        }
        std::vector<std::string> output_pins;
        for(const auto& pin : cell_pins["output"]) {
            output_pins.push_back(pin.as<std::string>());
        }

        const auto& pin_timing = cell_description["pin_timing"];
        assert(pin_timing.IsSequence());

        //Delay model for every pin
        for(const auto& pin_spec : pin_timing) {
            assert(pin_spec.IsMap());

            std::string from_pin = pin_spec["from_pin"].as<std::string>();
            std::string to_pin = pin_spec["to_pin"].as<std::string>();

            const auto& delays = pin_spec["delays"];
            assert(delays.IsMap());

            for(const auto& input_trans : valid_transitions) {

                if(!delays[input_trans]) continue;
                assert(delays[input_trans].IsMap());

                for(const auto& output_trans : valid_transitions) {
                    if(!delays[input_trans][output_trans]) continue;

                    //Key to uniqly identify this delay
                    auto delay_key = std::make_tuple(from_pin,to_pin,input_trans,output_trans);

                    assert(delay_model[cell_name].count(delay_key) == 0);

                    auto delay = delays[input_trans][output_trans];
                    assert(delay.IsScalar());

                    auto delay_value = delay.as<double>(); 

                    //Insert the delay value
                    delay_model[cell_name][delay_key] = delay_value;

                    std::cout << "\t" << from_pin << " -> " << to_pin << " (" << input_trans << " -> " << output_trans << "): " << delay_value << "\n";
                    
                }
            }

        }
    }

    return delay_model;
}

TransitionType trans_str_to_type(const std::string& trans_str) {
    TransitionType type;

    if(trans_str == "rise") {
        type = TransitionType::RISE;
    } else if (trans_str == "fall") {
        type = TransitionType::FALL;
    } else if (trans_str == "high") {
        type = TransitionType::HIGH;
    } else {
        assert(trans_str == "low");
        type = TransitionType::LOW;
    }

    return type;
}

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(DelayModel& delay_model, const TimingGraph& tg) {
    //TODO: currently hard coded for not (1-input) and and (2-input)

    PreCalcTransDelayCalculator::EdgeDelayModel edge_delay_model(tg.num_edges());

    for(NodeId node_id = 0; node_id < tg.num_nodes(); ++node_id) {
        auto node_type = tg.node_type(node_id);
        size_t num_in_edges = tg.num_node_in_edges(node_id);

        for(size_t i = 0; i < num_in_edges; ++i) {
            EdgeId edge_id = tg.node_in_edge(node_id, i);
            NodeId src_node_id = tg.edge_src_node(edge_id);
            auto src_node_type = tg.node_type(src_node_id);

            if(src_node_type == TN_Type::PRIMITIVE_IPIN && node_type == TN_Type::PRIMITIVE_OPIN) {
                CellDelayModel cell_model;
                if(num_in_edges == 1) {
                    auto cell_delay_model = delay_model["inv"];

                    assert(i == 0);

                    for(const auto& delay_pair : cell_delay_model) {
                        auto edge_delay = delay_pair.second;
                        auto input_trans = get<2>(delay_pair.first);
                        auto output_trans = get<3>(delay_pair.first);

                        auto edge_delay_key = std::make_tuple(trans_str_to_type(input_trans), trans_str_to_type(output_trans));
                        auto ret = edge_delay_model[edge_id].insert(std::make_pair(edge_delay_key,Time(edge_delay)));
                        assert(ret.second); //Was inserted
                    }


                } else if(num_in_edges == 2) {
                    auto cell_delay_model = delay_model["and"];

                    for(const auto& delay_pair : cell_delay_model) {
                        if((get<0>(delay_pair.first) == "a" && i != 0) || (get<0>(delay_pair.first) == "b" && i != 1)) continue;

                        auto edge_delay = delay_pair.second;
                        auto input_trans = get<2>(delay_pair.first);
                        auto output_trans = get<3>(delay_pair.first);

                        auto edge_delay_key = std::make_tuple(trans_str_to_type(input_trans), trans_str_to_type(output_trans));
                        auto ret = edge_delay_model[edge_id].insert(std::make_pair(edge_delay_key,Time(edge_delay)));
                        assert(ret.second); //Was inserted
                    }
                } else {
                    assert(false); //Unsupported primitives
                }

            } else {
                for(const auto& in_trans : valid_transitions) {
                    for(const auto& out_trans : valid_transitions) {
                        auto input_trans = trans_str_to_type(in_trans);
                        auto output_trans = trans_str_to_type(out_trans);

                        auto edge_delay_key = std::make_tuple(input_trans, output_trans);
                        auto ret = edge_delay_model[edge_id].insert(std::make_pair(edge_delay_key,Time(0.)));
                        assert(ret.second); //Was inserted
                    }
                }
            }
        }
    }


    return PreCalcTransDelayCalculator(edge_delay_model);
}
