#pragma once
#include <vector>
#include <string>
#include <unordered_map>

#include "vcd_parse.hpp"


/*
 *class Transition {
 *    public:
 *        enum class Type {
 *            RISE,
 *            FALL,
 *            HIGH,
 *            LOW,
 *            UNKOWN
 *        };
 *
 *        Transition(Type type_val, size_t time_val)
 *            : type_(type_val)
 *            , time_(time_val)
 *            {}
 *
 *        Type type() { return type_; }
 *        size_t time() { return time_; }
 *    private:
 *        size_t time_;
 *        Type type_;
 *}
 */

class VcdExtractor : public VcdCallback {
    
    public:
        VcdExtractor(std::string clock_name , std::vector<std::string> inputs, std::vector<std::string> outputs);

        void start() override;
        void finish() override;
        void add_var(std::string type, size_t width, std::string id, std::string name) override;
        void set_time(size_t time) override;
        void add_transition(std::string id, char val) override;

    private:
        void initialize_measure();
        void finalize_measure();

        std::string get_filename(const std::string& output_name);
        void write_header(const std::string& output_name);
        void write_transition(const std::string& output_name);
        char transition(char initial_value, char final_value);

    private:
        std::string clock_name_;
        std::vector<std::string> inputs_;
        std::vector<std::string> outputs_;

        std::unordered_map<std::string,std::string> id_to_name_;
        std::unordered_map<std::string,std::string> name_to_id_;

        std::unordered_map<std::string,std::tuple<char,size_t>> id_to_current_value_;
        std::unordered_map<std::string,std::tuple<char,size_t>> id_to_previous_value_;

        std::unordered_map<std::string,std::tuple<char,size_t>> id_to_measure_initial_value_;
        std::unordered_map<std::string,std::tuple<char,size_t>> id_to_measure_final_value_;

        size_t current_time_;
        size_t clock_rise_time_;

        size_t transition_count_;

        enum class State {
            INITIAL,
            CLOCK_RISE,
            CLOCK_FALL,
            MEASURE,
            SETUP,
        };

        State state_;
};

