#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <iterator>
#include <algorithm>
#include <set>

#include "vcdparse.hpp"


using namespace vcdparse;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::tie;
using std::tuple;
using std::unordered_map;

template class std::vector<TimeValue>;

//#define cilk_for _Cilk_for
#define cilk_for for

enum class PortType {
    INPUT,
    OUTPUT
};

class Transition {
    public:
        enum class Type {
            RISE=0,
            FALL=1,
            HIGH=2,
            LOW=3
        };
        Transition(Type type_val, size_t time_val)
            : type_(type_val)
            , time_(time_val)
            {}

        Type type() const { return type_; }
        size_t time() const { return time_; }

    private:
        Type type_;
        size_t time_;
};


bool operator<(Transition::Type lhs, Transition::Type rhs);
std::ostream& operator<<(std::ostream& os, Transition::Type type);

bool operator<(Transition::Type lhs, Transition::Type rhs) {
    return (int) lhs < (int) rhs;     
}

std::ostream& operator<<(std::ostream& os, Transition::Type type) {
    if(type == Transition::Type::RISE) os << "R";
    else if(type == Transition::Type::FALL) os << "F";
    else if(type == Transition::Type::HIGH) os << "H";
    else if(type == Transition::Type::LOW) os << "L";
    else assert(false);
    return os;
}


class DelayScenario {
    public:
        DelayScenario(const std::vector<Transition::Type>& input_trans_types,
                      const Transition& output_trans,
                      size_t delay_val)
            : input_transition_types_(input_trans_types)
            , output_transition_(output_trans)
            , delay_(delay_val) {
                assert(input_transition_types_.size() > 0); 
            }

        void print_csv_row(std::ostream& os) const {
            for(auto trans_type : input_transition_types_) {
                os << trans_type << ",";
            }
            os << output_transition_.type() << ",";
            os << delay_ << ",";
            os << output_transition_.time();
            os << "\n";
        }

        size_t num_input_transitions() const { return input_transition_types_.size(); }
        Transition::Type input_transition_type(size_t idx) const { return input_transition_types_.at(idx); }
        Transition::Type output_transition_type() const { return output_transition_.type(); }

    private:
        std::vector<Transition::Type> input_transition_types_;
        Transition output_transition_;
        size_t delay_;

};

tuple<string,string,vector<string>,vector<string>> parse_args(int argc, char** argv);


std::vector<TimeValue> find_time_values(const VcdData& vcd_data, std::string port_name);

Transition::Type transition(LogicValue prev, LogicValue next);

tuple<vector<size_t>,vector<size_t>> extract_edges(const std::vector<TimeValue>& time_values);

vector<Transition> extract_transitions(const VcdData& vcd_data, PortType port_type, string input_port, const vector<size_t>& rise_clock_edges, const vector<size_t>& fall_clock_edges);

vector<DelayScenario> extract_delay_scenarios(const vector<vector<Transition>>& all_input_transitions, const vector<Transition>& output_transitions, const vector<size_t>& rise_clock_edges, const vector<size_t>& fall_clock_edges);

void write_csv(std::ostream& os, vector<string> input_names, string output_name, vector<DelayScenario> delay_scenarios); 





tuple<string,string,vector<string>,vector<string>> parse_args(int argc, char** argv) {
    if(argc < 8) {
        cout << "Usage: \n";
        cout << "\t" << argv[0] << " vcd_file -c clock -i input_name [input_name...] -o output_name [output_name ...]\n";
        exit(1);
    }

    string vcd_file = argv[1];
    string clock = argv[3];

    vector<string> inputs;
    vector<string> outputs;

    int state = 0;
    for(int i = 4; i < argc; ++i) {
        string value = argv[i];

        if(value == "-i") state = 1;
        else if(value == "-o") state = 2;
        else if(state == 1) inputs.push_back(value);
        else if(state == 2) outputs.push_back(value);
        else assert(false);
    }

    return make_tuple(vcd_file, clock, inputs, outputs);
}

int main(int argc, char** argv) {
    string vcd_file;
    string clock_name;
    vector<string> input_names;
    vector<string> output_names;

    tie(vcd_file, clock_name, input_names, output_names) = parse_args(argc, argv);

    vcdparse::Loader vcd_loader;

    cout << "Loading VCD" << endl;
    bool loaded = vcd_loader.load(vcd_file);
    if(!loaded) {
        cout << "Failed to load VCD\n";
        return 1;
    }

    const VcdData& vcd_data = vcd_loader.get_vcd_data();

    //Extract clock transitions
    cout << "Extracting clock edges" << endl;
    auto clock_time_values = find_time_values(vcd_data, clock_name);
    vector<size_t> rise_clock_edges;
    vector<size_t> fall_clock_edges;
    tie(rise_clock_edges, fall_clock_edges) = extract_edges(clock_time_values);

    //Extract input transitions
    cout << "Extracting input transitions" << endl;
    vector<vector<Transition>> input_transitions(input_names.size());
    for(size_t i = 0; i < input_names.size(); ++i) {
        input_transitions[i] = extract_transitions(vcd_data, PortType::INPUT, input_names[i], rise_clock_edges, fall_clock_edges);
    }

    //Extract output transitions
    cout << "Extracting output transitions" << endl;
    vector<vector<Transition>> output_transitions(output_names.size());
    cilk_for(size_t i = 0; i < output_names.size(); ++i) {
        output_transitions[i] = extract_transitions(vcd_data, PortType::OUTPUT, output_names[i], rise_clock_edges, fall_clock_edges);
    }

    //Determine the delays for each input/output transition pair
    cout << "Determining transition delay scenarios" << endl;
    unordered_map<string,vector<DelayScenario>> output_delay_scenarios;
    for(auto output : output_names) { //Intialize
        output_delay_scenarios[output] = vector<DelayScenario>();
    }
    cilk_for(size_t i = 0; i < output_names.size(); ++i) {
        output_delay_scenarios[output_names[i]] = extract_delay_scenarios(input_transitions, output_transitions[i], rise_clock_edges, fall_clock_edges);
    }

    //Write the results
    cout << "Writting result CSVs" << endl;
    for(auto kv : output_delay_scenarios) {

        auto output_name = kv.first;

        //Get the basename
        size_t idx = vcd_file.find_last_of("/");
        if(idx == string::npos) {
            idx = 0;
        } else {
            idx += 1;
        }

        std::string csv_out(vcd_file.begin() + idx, vcd_file.end() - 4);
        csv_out += ".";
        csv_out += output_name;
        csv_out += ".csv";


        std::ofstream os(csv_out);

        std::cout << "  " << csv_out << std::endl;
        write_csv(os, input_names, output_name, kv.second); 

    }
    
    return 0;
}


template<typename T>
size_t find_index_lt(const std::vector<T>& values, size_t time) {
    //Finds the index of the first element in values with which occurs strictly before
    //time
    //
    //Assumes values is sorted
    auto cmp = [](const T& tv, size_t time_val){
        return tv.time() < time_val;
    };

    auto iter = std::lower_bound(values.begin(), values.end(), time, cmp);

    size_t idx = std::distance(values.begin(), iter-1);

    assert(values[idx].time() < time);
    if(idx+1 < values.size()) {
        assert(values[idx+1].time() >= time);
    }

    return idx;
}

template<typename T>
size_t find_index_ge(const std::vector<T>& values, size_t time) {
    //Finds the index of the first element in values which at or after time
    //
    //Assumes values is sorted
    auto cmp = [](const T& tv, size_t time_val){
        return tv.time() < time_val;
    };

    auto iter = std::lower_bound(values.begin(), values.end(), time, cmp);

    if(iter != values.end()) {

        size_t idx = std::distance(values.begin(), iter);

        auto val_time = values[idx].time();
        assert(val_time >= time);
        if(idx > 0) {
            auto val_time_past = values[idx-1].time();
            assert(val_time_past < time);
        }

        return idx;
    } else {
        throw std::runtime_error("no value found");
    }
}







std::vector<TimeValue> find_time_values(const VcdData& vcd_data, std::string port_name) {
    Var::Id id = -1;
    for(const auto& var : vcd_data.vars()) {
        if(var.name() == port_name) {
            id = var.id();
        }
    }
    assert(id != -1);

    std::vector<TimeValue> port_time_values;
    for(const auto& tv : vcd_data.time_values()) {
        if(tv.var_id() == id) {
            port_time_values.push_back(tv);
        }
    }

    return port_time_values;
}

tuple<vector<size_t>,vector<size_t>> extract_edges(const std::vector<TimeValue>& time_values) {
    vector<size_t> rise_edges;
    vector<size_t> fall_edges;

    LogicValue prev_val = LogicValue::ZERO;
    LogicValue next_val = LogicValue::ZERO;

    //Start from 1 to avoid un-initialized
    prev_val = time_values[0].value();
    for(size_t i = 1; i < time_values.size(); ++i) {
        next_val = time_values[i].value();

        auto trans = transition(prev_val, next_val);
        if(trans == Transition::Type::RISE) {
            rise_edges.push_back(time_values[i].time());
        } else {
            assert(trans == Transition::Type::FALL);
            fall_edges.push_back(time_values[i].time());
        }

        prev_val = next_val;
    }

    return make_tuple(rise_edges, fall_edges);
}

Transition::Type transition(LogicValue prev, LogicValue next) {
    if(prev == LogicValue::ZERO && next == LogicValue::ONE) {
        return Transition::Type::RISE;
    } else if (prev == LogicValue::ONE && next == LogicValue::ZERO) {
        return Transition::Type::FALL;
    } else if (prev == LogicValue::ONE && next == LogicValue::ONE) {
        return Transition::Type::HIGH;
    } else if (prev == LogicValue::ZERO && next == LogicValue::ZERO) {
        return Transition::Type::LOW;
    } else if (prev == LogicValue::UNKOWN && next == LogicValue::ONE) {
        return Transition::Type::RISE;
    } else if (prev == LogicValue::UNKOWN && next == LogicValue::ZERO) {
        return Transition::Type::FALL;
    }
    assert(false);
}

vector<Transition> extract_transitions(const VcdData& vcd_data, PortType port_type, string port, const vector<size_t>& rise_clock_edges, const vector<size_t>& fall_clock_edges) {
    vector<Transition> transitions;

    const std::vector<TimeValue>& time_values = find_time_values(vcd_data, port);
    auto sort_order = [](const TimeValue& lhs, const TimeValue& rhs) {
        return lhs.time() < rhs.time();
    };
    assert(std::is_sorted(time_values.begin(), time_values.end(), sort_order));

    //Walk through each of the clock cycles
    //to identify the corresponding transitions on this input
    for(size_t i = 0; i < rise_clock_edges.size()-1; ++i) {
        size_t launch_edge = rise_clock_edges[i];
        size_t capture_edge = fall_clock_edges[i+1];

        assert(launch_edge < capture_edge);

        //Find index to the value (strictly) before the launching edge
        size_t i_prev = find_index_lt(time_values, launch_edge);

        //Find the index to the value after any transitions in this cycle
        size_t i_next;
        if(port_type == PortType::INPUT) {
            //Find the next value at, or after the launch edge
            try {
                i_next = find_index_ge(time_values, launch_edge);
            } catch(std::runtime_error& e) {
                //No transition after this point (e.g. constant till end of simulation)
                i_next = i_prev;
            }

            //Check if the transition occurs outside the current clock cycle
            if(time_values[i_next].time() >= capture_edge) {
                i_next = i_prev; //Treat as steady value
            }

        } else {
            assert(port_type == PortType::OUTPUT);
            i_next = find_index_lt(time_values, capture_edge);
        }

        auto trans = transition(time_values[i_prev].value(), time_values[i_next].value());


        size_t time;
        if(port_type == PortType::INPUT) {
            time = launch_edge;
        } else {
            assert(port_type == PortType::OUTPUT);
            time = std::max(launch_edge, (size_t) time_values[i_next].time());

            assert(time >= launch_edge);
            assert(time < capture_edge);
        }
        transitions.emplace_back(trans, time);

        //if(launch_edge == 4294967664) {
            //std::cout << port << "\n";
            //std::cout << "\ti: " << i << "\n";
            //std::cout << "\tLaunch: " << launch_edge << "\n";
            //std::cout << "\tCapture: " << capture_edge << "\n";
            //std::cout << "\ti_prev: " << i_prev << " time: " << time_values[i_prev].time() << " value: " << time_values[i_prev].value()<< "\n";
            //std::cout << "\ti_next: " << i_next << " time: " << time_values[i_next].time() << " value: " << time_values[i_next].value()<< "\n";
            //std::cout << "\ttrans: " << trans << "\n";
            //std::cout << "\ttime: " << time << "\n";
            //std::cout << "\n";
        //}

    }

    return transitions;
}

vector<DelayScenario> extract_delay_scenarios(const vector<vector<Transition>>& all_input_transitions, const vector<Transition>& output_transitions, const vector<size_t>& rise_clock_edges, const vector<size_t>& fall_clock_edges) {
    
    vector<DelayScenario> output_delay_scenarios;

    for(size_t i = 0; i < rise_clock_edges.size()-1; ++i) {
        std::vector<Transition::Type> input_trans_types;

        size_t launch_edge = rise_clock_edges[i];
        size_t capture_edge = fall_clock_edges[i+1];

        assert(launch_edge < capture_edge);

        for(const auto& input_transitions : all_input_transitions) {
            
            assert(input_transitions.size() > 0);
            size_t j = find_index_ge(input_transitions, launch_edge);

            const auto& input_transition = input_transitions[j];

            //Must be equal to launch edge
            assert(input_transition.time() == launch_edge);

            input_trans_types.push_back(input_transition.type());
        }

        assert(output_transitions.size() > 0);

        size_t j = find_index_ge(output_transitions, launch_edge);
        const auto& output_transition = output_transitions[j];

        //Must be in the clock cycle
        assert(output_transition.time() >= launch_edge);
        assert(output_transition.time() < capture_edge);

        size_t delay = output_transition.time() - launch_edge;

        assert(delay < capture_edge - launch_edge);

        assert(input_trans_types.size() == all_input_transitions.size());

        DelayScenario new_delay_scenario(input_trans_types, output_transition, delay);
        output_delay_scenarios.push_back(new_delay_scenario); 
    }

    //Radix-style stable sort -- very slow
    for(int i = (int) all_input_transitions.size() - 1; i >= 0; --i) {
        auto sort_order = [&](const DelayScenario& lhs, const DelayScenario& rhs) {
            return lhs.input_transition_type(i) < rhs.input_transition_type(i);
        };
        std::stable_sort(output_delay_scenarios.begin(), output_delay_scenarios.end(), sort_order);
    }

    return output_delay_scenarios;
}

void write_csv(std::ostream& os, vector<string> input_names, string output_name, vector<DelayScenario> delay_scenarios) {
    //The header
    for(auto input_name : input_names) {
        os << input_name << ",";
    }
    os << output_name << ",";
    os << "delay" << ",";
    os << "sim_time" << "\n";

    //The body
    for(const auto& scenario : delay_scenarios) {
        scenario.print_csv_row(os);
    }
}
