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

BlifModel* BlifData::find_model(std::string* model_name) const {
    for(BlifModel* model : models) {
        if(model->name == model_name) {
            return model;
        }
    }
    return nullptr;
}

bool BlifData::verify() const {
    for(auto model : models) {
        if(!model->verify()) return false;
    }
    return true;
}

/*
 * BlifModel
 */
BlifPort* BlifModel::find_input_port(std::string* port_name) {
    for(BlifPort* port : inputs) {
        if(port->name == port_name) {
            return port;
        }
    }
    return nullptr;
}

BlifPort* BlifModel::find_output_port(std::string* port_name) {
    for(BlifPort* port : outputs) {
        if(port->name == port_name) {
            return port;
        }
    }
    return nullptr;
}

BlifPort* BlifModel::find_clock_port(std::string* port_name) {
    for(BlifPort* port : clocks) {
        if(port->name == port_name) {
            return port;
        }
    }
    return nullptr;
}

bool BlifModel::is_input_port(std::string* port_name) {
    return nullptr != find_input_port(port_name);
}

bool BlifModel::is_output_port(std::string* port_name) {
    return nullptr != find_output_port(port_name);
}

bool BlifModel::is_clock_port(std::string* port_name) {
    return nullptr != find_clock_port(port_name);
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

bool BlifModel::verify() {
    //Basic checks for now, ensure that each net is consistent
    for(auto net : nets) {
        for(auto driver_conn : net->drivers) {
            if(!verify_port_conn_consistent(driver_conn)) {
                return false;
            }
        }

        for(auto sink_conn : net->sinks) {
            if(!verify_port_conn_consistent(sink_conn)) {
                return false;
            }
        }
    }

    return true;
}

bool BlifModel::verify_port_conn_consistent(BlifPortConn* port_conn) {
    BlifPort* port_conn_port = port_conn->port;
    std::vector<BlifPort*> ports;
    if(port_conn_port->node_type == BlifNodeType::NAMES) {
        BlifNames* names_obj = port_conn_port->names;
        ports = names_obj->ports;
    } else if(port_conn_port->node_type == BlifNodeType::SUBCKT) {
        BlifSubckt* subckt = port_conn_port->subckt;
        ports = subckt->ports;
    } else if(port_conn_port->node_type == BlifNodeType::LATCH) {
        BlifLatch* latch = port_conn_port->latch;
        ports.push_back(latch->input);
        ports.push_back(latch->output);
        ports.push_back(latch->control);

    } else if(port_conn_port->node_type == BlifNodeType::MODEL) {
        BlifModel* model = port_conn_port->model;

        auto iter = std::copy(model->inputs.begin(), model->inputs.end(), std::back_inserter(ports));
        iter = std::copy(model->outputs.begin(), model->outputs.end(), iter);
        iter = std::copy(model->clocks.begin(), model->clocks.end(), iter);
    }

    //Check that the port_conn matches the one used in the ports
    bool seen_port_conn = false;
    for(auto port : ports) {
        if(port->port_conn == port_conn) {
            seen_port_conn = true;
        }
    }
    assert(seen_port_conn);
    return seen_port_conn;
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

