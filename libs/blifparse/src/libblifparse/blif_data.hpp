#pragma once
#include <vector>
#include <string>
#include <iosfwd>
#include <cassert>
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

enum class BlifNodeType {
    NAMES,
    LATCH,
    SUBCKT,
    MODEL
};

enum class NetTermType {
    DRIVER,
    SINK
};

//Forward declarations
struct BlifPort;
struct BlifPortConn;
struct BlifNet;

struct BlifModel;
struct BlifNames;
struct BlifLatch;
struct BlifSubckt;

struct BlifData {
    //List of models
    std::vector<BlifModel*> models;


    //String table
    StringTable str_table;

    const BlifModel* get_top_model() const { return models[0]; };
    BlifModel* get_top_model() { return models[0]; }

    void resolve_nets();

    void clean_nets();

    void sweep_dangling_ios();

    BlifModel* find_model(std::string* model_name);

    ~BlifData();
};

struct BlifPortConn {
    BlifPortConn(BlifPort* port_, BlifNet* net_)
        : port(port_)
        , net(net_) {}
    BlifPort* port;
    BlifNet* net;
};

struct BlifPort {
    BlifPort(std::string* name_, BlifNames* names_)
        : name(name_)
        , node_type(BlifNodeType::NAMES)
        , names(names_)
        , port_conn(nullptr) {}
    BlifPort(std::string* name_, BlifLatch* latch_)
        : name(name_)
        , node_type(BlifNodeType::LATCH)
        , latch(latch_)
        , port_conn(nullptr) {}
    BlifPort(std::string* name_, BlifSubckt* subckt_)
        : name(name_)
        , node_type(BlifNodeType::SUBCKT)
        , subckt(subckt_)
        , port_conn(nullptr) {}
    BlifPort(std::string* name_, BlifModel* model_)
        : name(name_)
        , node_type(BlifNodeType::MODEL)
        , model(model_)
        , port_conn(nullptr) {}
    std::string* name;
    BlifNodeType node_type;
    union {
        BlifNames* names;
        BlifLatch* latch;
        BlifSubckt* subckt;
        BlifModel* model;
    };
    BlifPortConn* port_conn;

    ~BlifPort() {
        //Only responsible for the port_conn
        delete port_conn;
    }
};

struct BlifNet {
    BlifNet(std::string* name_): name(name_) {}
    std::string* name;
    std::vector<BlifPortConn*> drivers;
    std::vector<BlifPortConn*> sinks;
};

struct BlifNames {
    std::vector<BlifPort*> ports; //List of inputs and output. Inputs: [0..ports.size()-2]. Output: [ports.size()-1]
    std::vector<std::vector<LogicValue>*> cover_rows; //List of rows in covers [0..cover_rows.size()-1][0..ports.size()-1]

    ~BlifNames() {
        //only need to clean-up non-strings
        for(auto row_ptr : cover_rows) {
            delete row_ptr;
        }
        for(auto port : ports) {
            delete port;
        }
    }
};

struct BlifLatch {
    BlifPort* input;
    BlifPort* output;
    LatchType type;
    BlifPort* control;
    LogicValue initial_state;
};

struct BlifSubckt {
    BlifSubckt(std::string* type_): type(type_) {}

    std::string* type;
    std::vector<BlifPort*> ports;

    ~BlifSubckt() {
        for(auto port_ptr : ports) {
            delete port_ptr;
        }
    }
};


struct BlifModel {
    BlifModel(std::string* name_)
        : name(name_)
        , blackbox(false)
        , ended(false) {}

    std::string* name;
    std::vector<BlifPort*> inputs;
    std::vector<BlifPort*> outputs;
    std::vector<BlifPort*> clocks;

    std::vector<BlifNames*> names;
    std::vector<BlifLatch*> latches;
    std::vector<BlifSubckt*> subckts;

    std::vector<BlifNet*> nets;

    bool blackbox;
    bool ended;
    std::unordered_map<std::string*,BlifNet*> net_name_lookup;

    bool is_input_port(std::string* port_name);
    bool is_output_port(std::string* port_name);

    BlifNet* get_net(std::string* net_name);

    void clean_nets();
    void sweep_dangling_ios();

    void resolve_nets(BlifData* blif_data);
    void update_net(BlifPortConn* port_conn, NetTermType term_type);

    ~BlifModel();
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
