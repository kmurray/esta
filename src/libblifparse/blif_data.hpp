#pragma once
#include <vector>
#include <string>
#include <iosfwd>
#include "StringTable.hpp"

enum class LogicValue {
    FALSE = 0,  //Logic zero
    TRUE = 1,   //Logic one
    DC,     //Don't care
    UNKOWN  //Unkown (e.g. latch initial state)
};
std::ostream& operator<<(std::ostream& os, const LogicValue& val);

enum class LatchType {
    FALLING_EDGE,
    RISING_EDGE,
    ACTIVE_HIGH,
    ACTIVE_LOW,
    ASYNCHRONOUS,
    UNSPECIFIED //If no type is specified
};
std::ostream& operator<<(std::ostream& os, const LatchType& val);

struct BlifNames {
    std::vector<std::string*> ios; //List of inputs and output. Inputs: [0..ios.size()-2]. Output: [ios.size()-1]
    std::vector<std::vector<LogicValue>*> cover_rows; //List of rows in covers [0..cover_rows.size()-1][0..ios.size()-1]

    ~BlifNames() {
        //only need to clean-up non-strings
        for(auto row_ptr : cover_rows) {
            delete row_ptr;
        }
    }
};

struct BlifLatch {
    std::string* input;
    std::string* output;
    LatchType type;
    std::string* control;
    LogicValue initial_state;
};

struct PortConnection {
    std::string* port_name;
    std::string* signal_name;
};

struct BlifSubckt {
    std::string* type;
    std::vector<PortConnection*> port_connections;

    ~BlifSubckt() {
        for(auto pc_ptr : port_connections) {
            delete pc_ptr;
        }
    }
};


struct BlifModel {
    std::string* model_name;
    std::vector<std::string*> inputs;
    std::vector<std::string*> outputs;
    std::vector<std::string*> clocks;

    std::vector<BlifNames*> names;
    std::vector<BlifLatch*> latches;
    std::vector<BlifSubckt*> subckts;
    bool blackbox = false;
    bool ended = false;

    ~BlifModel() {
        for(auto names_ptr : names) {
            delete names_ptr;
        }

        for(auto latch_ptr : latches) {
            delete latch_ptr;
        }

        for(auto subckt_ptr : subckts) {
            delete subckt_ptr;
        }
    }
};

struct BlifData {
    //List of models
    std::vector<BlifModel*> models;

    //String table
    StringTable str_table;

    ~BlifData() {
        for(auto model_ptr: models) {
            delete model_ptr;
        }
    }
};


/*
 * Internally used parsing types
 */

struct LatchTypeControl {
    LatchType type;
    std::string* control;
};


/*
 *  Externally useful functions
 */
/*
 *bool blif_parse_filename(BlifData* blif_data, char* blif_filename);
 *bool blif_parse_file(BlifData* blif_data, FILE* blif_file);
 *
 *void blif_parse_cleanup();
 *
 */
