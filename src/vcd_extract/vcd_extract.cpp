#include <iostream>
#include <fstream>
#include <cassert>
#include <limits>
#include <iomanip>

#include "gzstream.h"

using std::cout;
using std::endl;

#include "vcd_extract.hpp"

VcdExtractor::VcdExtractor(std::string clock_name, std::vector<std::string> inputs, std::vector<std::string> outputs, std::string output_dir)
    : clock_name_(clock_name)
    , inputs_(inputs)
    , outputs_(outputs)
    , output_dir_(output_dir)
    , transition_count_(0) 
    , state_(State::INITIAL)
{
    outputs_os_ = std::make_shared<ogzstream>(get_trans_filename().c_str());  
    *outputs_os_ << std::setprecision(std::numeric_limits<double>::digits10);

    max_os_ = std::make_shared<ogzstream>(get_max_filename().c_str());  
    *max_os_ << std::setprecision(std::numeric_limits<double>::digits10);

    write_trans_header();
    write_max_header();
}

void VcdExtractor::start() {
    cout << "Starting VCD..." << endl;
}

void VcdExtractor::finish() {
    cout << "Finished VCD (" << transition_count_ << " transitions)" << endl;
}

void VcdExtractor::add_var(std::string type, size_t width, std::string id, std::string name) {
    //cout << "Adding var '" << id << "' (" << name << ")" << endl;

    id_to_name_[id] = name;
    name_to_id_[name] = id;

    //Initialize variable states
    id_to_previous_value_[id] = std::make_tuple('x', 0);
}

void VcdExtractor::set_time(size_t time) {
    //Called whenever the time-step advances

    //We handle anything that needs doing before advancing
    //the timestep
    if(state_ == State::CLOCK_RISE) {
        initialize_measure();
        state_ = State::MEASURE;
        //std::cout << "State: MEASURE" << std::endl;
    } else if (state_ == State::CLOCK_FALL) {
        finalize_measure();
        state_ = State::SETUP;
        //std::cout << "State: SETUP" << std::endl;
    }

    //Advance to next timestep
    current_time_ = time;

    //Save the values from the previous timestep
    //for(auto& kv : id_to_current_value_) {
        //id_to_previous_value_[kv.first] = kv.second;
    //}
    id_to_previous_value_.insert(id_to_current_value_.begin(), id_to_current_value_.end());
    id_to_current_value_.clear();
}

void VcdExtractor::add_transition(std::string id, char val) {
    //cout << "Transition: '" << id << "' " << val << endl;
    ++transition_count_;

    if(transition_count_ % 1000000 == 0) {
        cout << "Processed " << transition_count_ / 1e6 << "M transitions" << endl;
    }

    //cout << id << "(" << id_to_name_[id] << "): " << val << endl;

    if(id_to_name_[id] == clock_name_) {
        if(std::get<0>(id_to_previous_value_[id]) == '0' && val == '1') {
            //cout << "Clock R @ " << current_time_ << ": " << std::get<0>(id_to_previous_value_[id]) << " -> " << val << endl;

            state_ = State::CLOCK_RISE;
            //std::cout << "State: CLOCK_RISE" << std::endl;
        } else if(std::get<0>(id_to_previous_value_[id]) == '1' && val == '0') {
            //cout << "Clock F @ " << current_time_ << ": " << std::get<0>(id_to_previous_value_[id]) << " -> " << val << endl;

            if(state_ != State::INITIAL) { //Protect from spurious falling edges during startup
                state_ = State::CLOCK_FALL;
                //std::cout << "State: CLOCK_FALL" << std::endl;
            }
        } else {
            //cout << "Clock X @ " << current_time_ << ": " << std::get<0>(id_to_previous_value_[id]) << " -> " << val << endl;
            assert(state_ == State::INITIAL);
        }
    }

    id_to_current_value_[id] = std::make_tuple(val, current_time_); 
}

void VcdExtractor::initialize_measure() {
    //Save the intial state just before the rising edge of the clock
    //
    //We use the previous timestep values, since we want to look at things just before
    //the falling edge
    id_to_measure_initial_value_ = id_to_previous_value_;

    clock_rise_time_ = current_time_;
}

void VcdExtractor::finalize_measure() {
    //Save the final state before the falling edge of the clock
    //
    //We use the previous timestep values, since we want to look at things just before
    //the falling edge
    id_to_measure_final_value_ = id_to_previous_value_;


    write_transition();
    write_max_transition();
}

std::string VcdExtractor::get_trans_filename() {
    return output_dir_ + "/sim.trans.csv.gz";
}

std::string VcdExtractor::get_max_filename() {
    return output_dir_ + "/sim.max_trans.csv.gz";
}

void VcdExtractor::write_trans_header() {

    for(const auto& input_name : inputs_) {
        *outputs_os_ << input_name << ",";
    }
    for(size_t i = 0; i < outputs_.size(); ++i) {
        *outputs_os_ << outputs_[i] << ",";
        *outputs_os_ << "delay" << ":" << outputs_[i] << ",";
        *outputs_os_ << "sim_time" << ":" << outputs_[i];
        if(i != outputs_.size() - 1) {
            *outputs_os_ << ",";
        }
    }
    *outputs_os_ << "\n";
}

void VcdExtractor::write_max_header() {

    for(const auto& input_name : inputs_) {
        *max_os_ << input_name << ",";
    }
    *max_os_ << "MAX" << ",";
    *max_os_ << "delay:MAX" << ",";
    *max_os_ << "sim_time:MAX";
    *max_os_ << "\n";
}

void VcdExtractor::write_transition() {

    //Write the row to the CSV

    //Inputs
    for(const auto& input_name : inputs_) {
        std::string id = name_to_id_[input_name];

        char initial_value = std::get<0>(id_to_measure_initial_value_[id]);
        char final_value = std::get<0>(id_to_measure_final_value_[id]);

        char input_trans = transition(initial_value, final_value);
        *outputs_os_ << input_trans << ",";
    }

    //Outputs
    for(size_t i = 0; i < outputs_.size(); ++i) {

        //Output transition
        std::string id = name_to_id_[outputs_[i]];
        char initial_value = std::get<0>(id_to_measure_initial_value_[id]);
        char final_value = std::get<0>(id_to_measure_final_value_[id]);

        *outputs_os_ << transition(initial_value, final_value) << ",";

        //Delay
        size_t delay;
        size_t sim_time;
        std::tie(delay, sim_time) = output_delay(id);

        *outputs_os_ << delay << ",";

        //Sim time (launch clock)
        *outputs_os_ << sim_time;

        if(i != outputs_.size() - 1) {
            *outputs_os_ << ",";
        }
    }
    *outputs_os_ << "\n";
}

void VcdExtractor::write_max_transition() {
    //Inputs
    for(const auto& input_name : inputs_) {
        std::string id = name_to_id_[input_name];

        char initial_value = std::get<0>(id_to_measure_initial_value_[id]);
        char final_value = std::get<0>(id_to_measure_final_value_[id]);

        char input_trans = transition(initial_value, final_value);
        *max_os_ << input_trans << ",";
    }

    //Output transition
    *max_os_ << "-" << ",";

    size_t delay = 0;
    size_t sim_time = 0;

    //Take the max over all the outputs
    for(const auto& output_name : outputs_) {
        std::string id = name_to_id_[output_name];

        size_t out_delay;
        size_t out_sim_time;
        std::tie(out_delay, out_sim_time) = output_delay(id);

        if(out_delay >= delay) {
            delay = out_delay;
            sim_time = out_sim_time;
        }
    }

    *max_os_ << delay << ",";

    *max_os_ << sim_time << "\n";

}

char VcdExtractor::transition(char initial_value, char final_value) {
    if(initial_value == 'x' || final_value == 'x') {
        return 'X';
    } else if(initial_value == '1' && final_value == '1') {
        return 'H';
    } else if(initial_value == '1' && final_value == '0') {
        return 'F';
    } else if(initial_value == '0' && final_value == '1') {
        return 'R';
    } else if(initial_value == '0' && final_value == '0') {
        return 'L';
    } else {
        throw std::runtime_error("Unrecognized_transition");
        assert(false && "Unrecognized transition");
    }
}

std::pair<size_t,size_t> VcdExtractor::output_delay(const std::string& id) {
    size_t output_trans_time = std::get<1>(id_to_measure_final_value_[id]);

    size_t delay;
    size_t sim_time;
    if(output_trans_time < clock_rise_time_) {
        delay = 0;
        sim_time = clock_rise_time_;
    } else {
        delay = output_trans_time - clock_rise_time_;
        sim_time = output_trans_time;
    }

    return std::make_pair(delay,sim_time);
}
