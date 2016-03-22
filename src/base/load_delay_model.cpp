#include <iostream>
#include <iterator>
#include <fstream>
#include <vector>
#include <cassert>

#include <yaml-cpp/yaml.h>

#include "load_delay_model.hpp"

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

