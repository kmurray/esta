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

    bool is_input_port(std::string* port_name) {
        for(BlifPort* port : inputs) {
            if(port->name == port_name) {
                return true;
            }
        }
        return false;
    }

    bool is_output_port(std::string* port_name) {
        for(BlifPort* port : outputs) {
            if(port->name == port_name) {
                return true;
            }
        }
        return false;
    }

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

        for(auto port_list : {inputs, outputs, clocks}) {
            for(auto port_ptr : port_list) {
                delete port_ptr;
            }
        }
    }
};

struct BlifData {
    //List of models
    std::vector<BlifModel*> models;
    std::vector<BlifNet*> nets;

    std::unordered_map<std::string*,BlifNet*> net_name_lookup;

    //String table
    StringTable str_table;

    BlifNet* get_net(std::string* name) {
        auto iter = net_name_lookup.find(name);
        if(iter != net_name_lookup.end()) {
            return iter->second;
        } else {
            BlifNet* net = new BlifNet(name);

            net_name_lookup[name] = net;
            nets.push_back(net);

            return net;
        }
    }

    BlifModel* get_top_model() const { return models[0]; };
    BlifModel* get_top_model() { return models[0]; }

    void resolve_nets() {
        //Walk through all the ports on all primitives
        //adding the terminals (as drivers/sinks) to
        //their connected nets

        //We only walk the first (main) model
        BlifModel* model = models[0];
        //Model inputs
        for(BlifPort* input : model->inputs) {
            BlifPortConn* conn = input->port_conn;
            update_net(conn, NetTermType::DRIVER);
        }

        //Model outputs
        for(BlifPort* output : model->outputs) {
            BlifPortConn* conn = output->port_conn;
            update_net(conn, NetTermType::SINK);
        }

        //Model clocks
        for(BlifPort* clock : model->clocks) {
            BlifPortConn* conn = clock->port_conn;
            update_net(conn, NetTermType::DRIVER);
        }

        //Names
        for(BlifNames* names : model->names) {
            for(size_t i = 0; i < names->ports.size(); i++) {
                BlifPortConn* conn = names->ports[i]->port_conn;
                if(i < names->ports.size() - 1) {
                    update_net(conn, NetTermType::SINK);
                } else {
                    update_net(conn, NetTermType::DRIVER);
                }
            }
        }

        //Latches
        for(BlifLatch* latch : model->latches) {
            BlifPortConn* input_conn = latch->input->port_conn;
            BlifPortConn* output_conn = latch->output->port_conn;
            BlifPortConn* control_conn = latch->control->port_conn;

            update_net(input_conn, NetTermType::SINK);
            update_net(output_conn, NetTermType::DRIVER);
            update_net(control_conn, NetTermType::SINK);
        }

        //Subckts
        for(BlifSubckt* subckt : model->subckts) {
            //We need to lookup based on the associated subckt model
            //which ports are inputs or outputs
            BlifModel* subckt_model = find_model(subckt->type);
            assert(subckt_model != nullptr);

            for(BlifPort* port : subckt->ports) {
                BlifPortConn* conn = port->port_conn;

                if(subckt_model->is_input_port(port->name)) {
                    update_net(conn, NetTermType::SINK);

                } else if (subckt_model->is_output_port(port->name)) {
                    update_net(conn, NetTermType::DRIVER);

                } else {
                    assert(0);
                }
            }
        }
    }

    void clean_nets() {
        //Walk through the blif file and remove nets with no
        //drivers or no sinks
         
        //To avoid random removal in a vector,
        //we build seperate vectors to track nets
        std::vector<BlifNet*> nets_to_remove;
        std::vector<BlifNet*> nets_to_keep;

        //Categorize nets
        for(BlifNet* net : nets) {
            if(net->sinks.size() == 0 || net->drivers.size() == 0) {
                nets_to_remove.push_back(net);
            } else {
                nets_to_keep.push_back(net);
            }
        }

        //Remove nets
        for(BlifNet* net : nets_to_remove) {
            for(BlifPortConn* conn : net->drivers) {
                BlifPort* port = conn->port;;
                port->port_conn = nullptr;
                delete conn;
            }

            for(BlifPortConn* conn : net->sinks) {
                BlifPort* port = conn->port;
                port->port_conn = nullptr;
                delete conn;
            }

            delete net;
        }

        //Update the original set of nets with only the set we are keeping
        nets = nets_to_keep;
    }

    void sweep_dangling_ios(BlifModel* model) {
        std::vector<BlifPort*> inputs_to_keep;
        for(BlifPort* in_port : model->inputs) {
            if(in_port->port_conn) {
                //This input drives a net, keep it     
                inputs_to_keep.push_back(in_port);
            } else {
                //This input does NOT drive a net, remove it     
                delete in_port;
            }
        }
        model->inputs = inputs_to_keep;

        std::vector<BlifPort*> outputs_to_keep;
        for(BlifPort* out_port : model->outputs) {
            if(out_port->port_conn) {
                //This input drives a net, keep it     
                outputs_to_keep.push_back(out_port);
            } else {
                //This input does NOT drive a net, remove it     
                delete out_port;
            }
        }
        model->outputs = outputs_to_keep;
    }

    BlifModel* find_model(std::string* model_name) {
        for(BlifModel* model : models) {
            if(model->name == model_name) {
                return model;
            }
        }
        return nullptr;
    }

    void update_net(BlifPortConn* port_conn, NetTermType term_type) {
        //Update the terminals of the net associated with port_conn
        //to include it in its list of drivers/sinks
        if(port_conn != nullptr) {
            BlifNet* net = port_conn->net;
            if(term_type == NetTermType::DRIVER) {
                net->drivers.push_back(port_conn);
            } else {
                assert(term_type == NetTermType::SINK);
                net->sinks.push_back(port_conn);
            }
        }
    }

    ~BlifData() {
        for(auto model_ptr: models) {
            delete model_ptr;
        }
        for(auto net_ptr : nets) {
            delete net_ptr;
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
