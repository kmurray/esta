#include <cassert>
#include <iostream>
#include <string>
#include "BlifParseError.hpp"
#include "blif_data.hpp"
#include "blif_parse_common.hpp"

/*
 * BlifData
 */
BlifData::~BlifData() {
    for(auto model_ptr: models) {
        delete model_ptr;
    }
}

void BlifData::resolve_nets() {
    for(BlifModel* model : models) {
        model->resolve_nets(this);
    }
}

void BlifData::clean_nets() {
    for(BlifModel* model : models) {
        model->clean_nets();
    }

}

void BlifData::sweep_dangling_ios() {
    for(BlifModel* model : models) {
        model->sweep_dangling_ios();
    }
}

BlifModel* BlifData::find_model(std::string* model_name) {
    for(BlifModel* model : models) {
        if(model->name == model_name) {
            return model;
        }
    }
    return nullptr;
}

/*
 * BlifModel
 */
bool BlifModel::is_input_port(std::string* port_name) {
    for(BlifPort* port : inputs) {
        if(port->name == port_name) {
            return true;
        }
    }
    return false;
}

bool BlifModel::is_output_port(std::string* port_name) {
    for(BlifPort* port : outputs) {
        if(port->name == port_name) {
            return true;
        }
    }
    return false;
}

BlifNet* BlifModel::get_net(std::string* net_name) {
    auto iter = net_name_lookup.find(net_name);
    if(iter != net_name_lookup.end()) {
        return iter->second;
    } else {
        BlifNet* net = new BlifNet(net_name);

        net_name_lookup[net_name] = net;
        nets.push_back(net);
        return net;
    }
}

void BlifModel::clean_nets() {
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

void BlifModel::sweep_dangling_ios() {
    std::vector<BlifPort*> inputs_to_keep;
    for(BlifPort* in_port : inputs) {
        if(in_port->port_conn) {
            //This input drives a net, keep it     
            inputs_to_keep.push_back(in_port);
        } else {
            //This input does NOT drive a net, remove it     
            delete in_port;
        }
    }
    inputs = inputs_to_keep;

    std::vector<BlifPort*> outputs_to_keep;
    for(BlifPort* out_port : outputs) {
        if(out_port->port_conn) {
            //This input drives a net, keep it     
            outputs_to_keep.push_back(out_port);
        } else {
            //This input does NOT drive a net, remove it     
            delete out_port;
        }
    }
    outputs = outputs_to_keep;
}

void BlifModel::resolve_nets(BlifData* blif_data) {
    //Walk through all the ports on all primitives
    //adding the terminals (as drivers/sinks) to
    //their connected nets

    //Model inputs
    for(BlifPort* input : inputs) {
        BlifPortConn* conn = input->port_conn;
        update_net(conn, NetTermType::DRIVER);
    }

    //Model outputs
    for(BlifPort* output : outputs) {
        BlifPortConn* conn = output->port_conn;
        update_net(conn, NetTermType::SINK);
    }

    //Model clocks
    for(BlifPort* clock : clocks) {
        BlifPortConn* conn = clock->port_conn;
        update_net(conn, NetTermType::DRIVER);
    }

    //Names
    for(BlifNames* names_obj : names) {
        for(size_t i = 0; i < names_obj->ports.size(); i++) {
            BlifPortConn* conn = names_obj->ports[i]->port_conn;
            if(i < names_obj->ports.size() - 1) {
                update_net(conn, NetTermType::SINK);
            } else {
                update_net(conn, NetTermType::DRIVER);
            }
        }
    }

    //Latches
    for(BlifLatch* latch : latches) {
        BlifPortConn* input_conn = latch->input->port_conn;
        BlifPortConn* output_conn = latch->output->port_conn;
        BlifPortConn* control_conn = latch->control->port_conn;

        update_net(input_conn, NetTermType::SINK);
        update_net(output_conn, NetTermType::DRIVER);
        update_net(control_conn, NetTermType::SINK);
    }

    //Subckts
    for(BlifSubckt* subckt : subckts) {
        //We need to lookup based on the associated subckt model
        //which ports are inputs or outputs
        BlifModel* subckt_model = blif_data->find_model(subckt->type);

        if(subckt_model == nullptr) {
            throw BlifParseError("Could not resolve subckt '" + *subckt->type + "'"); 
        }

        for(BlifPort* port : subckt->ports) {
            BlifPortConn* conn = port->port_conn;

            if(subckt_model->is_input_port(port->name)) {
                update_net(conn, NetTermType::SINK);

            } else if (subckt_model->is_output_port(port->name)) {
                update_net(conn, NetTermType::DRIVER);

            } else {
                throw BlifParseError("For subckt '" + *subckt->type + "' could not resolve type for port '" + *port->name + "'"); 
            }
        }
    }
}

void BlifModel::update_net(BlifPortConn* port_conn, NetTermType term_type) {
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

BlifModel::~BlifModel() {
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
    for(auto net_ptr : nets) {
        delete net_ptr;
    }
}


/*
 * Stream operators
 */
std::ostream& operator<<(std::ostream& os, const LogicValue& val) {
    switch(val) {
        case LogicValue::TRUE:
            os << "1";
            break;
        case LogicValue::FALSE:
            os << "0";
            break;
        case LogicValue::DC:
            os << "-";
            break;
        case LogicValue::UNKOWN:
            os << "X";
            break;
        default:
            assert(0);
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const LatchType& val) {
    switch(val) {
        case LatchType::FALLING_EDGE:
            os << "fe";
            break;
        case LatchType::RISING_EDGE:
            os << "re";
            break;
        case LatchType::ACTIVE_LOW:
            os << "al";
            break;
        case LatchType::ACTIVE_HIGH:
            os << "ah";
            break;
        case LatchType::ASYNCHRONOUS:
            os << "as";
            break;
        default:
            assert(0);
    }
    return os;
}

