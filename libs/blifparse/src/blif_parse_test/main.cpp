#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>

#include "blif_parse.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

void print_blif_nets(const BlifData* blif_data);
void print_blif(const BlifData* blif_data);

void print_blif_model(const BlifModel* model);
void print_blif_names(const BlifNames* names);
void print_blif_latch(const BlifLatch* latch);
void print_blif_subckt(const BlifSubckt* subckt);


int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "Usage: " << argv[0] << " filename1.blif [filename2.blif [filename3.blif [...]]]" << endl;
        return 1;
    }

    for(int i = 1; i < argc; i++) {
        cerr << "Parsing file: " << argv[i] << endl;

        //Create the parser
        BlifParser parser;

        //Load the file
        BlifData* blif_data = nullptr;
        try {
            blif_data = parser.parse(argv[i]);;
        } catch (BlifParseError& e) {
            cerr << argv[i] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
            return 1;
        }

        if(blif_data != nullptr) {
            //Walk and print the blif
            //print_blif(blif_data);
            //print_blif_nets(blif_data);
            cerr << "\tOK" << endl;
            delete blif_data;
        }
    }

    return 0;
}

void print_blif_nets(const BlifData* blif_data) {
    for(BlifNet* net : blif_data->nets) {
        cout << "Net: " << *net->name << "\n";
        cout << "\tDrivers: " << net->drivers.size() << "\n";
/*
 *        for(BlifPortConn* conn : net->drivers) {
 *            cout << "\t\t";
 *
 *        }
 */
        cout << "\tSinks: " << net->sinks.size() << "\n";
    }
}

void print_blif(const BlifData* blif_data) {
    for(const BlifModel* model : blif_data->models) {
        print_blif_model(model);
        cout << "\n";
    }
    cout << endl;
}

void print_blif_model(const BlifModel* model) {
    cout << ".model " << *(model->name) << "\n";

    if(model->inputs.size() > 0) {
        cout << ".inputs";
        for(auto port : model->inputs) {
            cout << " " << *port->name;
        }
        cout << "\n";
    }

    if(model->outputs.size() > 0) {
        cout << ".outputs";
        for(auto port : model->outputs) {
            cout << " " << *port->name;
        }
        cout << "\n";
    }

    if(model->clocks.size() > 0) {
        cout << ".clocks";
        for(auto port : model->clocks) {
            cout << " " << *port->name;
        }
        cout << "\n";
    }

    if(model->names.size() > 0) {
        cout << "\n";
        for(const BlifNames* blif_names : model->names) {
            print_blif_names(blif_names);
            cout << "\n";
        }
    }

    if(model->latches.size() > 0) {
        cout << "\n";
        for(const BlifLatch* blif_latch : model->latches) {
            print_blif_latch(blif_latch);
        }
    }

    if(model->subckts.size() > 0) {
        cout << "\n";
        for(const BlifSubckt* blif_subckt : model->subckts) {
            print_blif_subckt(blif_subckt);
            cout << "\n";
        }
    }
    if(model->blackbox) {
        cout << ".blackbox\n";
    }
    if(model->ended) {
        cout << ".end\n";
    }
}

void print_blif_names(const BlifNames* names) {
    cout << ".names";
    for(auto port : names->ports) {
        cout << " " << *port->name;
    }
    cout << "\n";

    for(const auto& row : names->cover_rows) {
        for(size_t i = 0; i < row->size(); i++) {
            LogicValue val = (*row)[i];

            cout << val;

            //Space between inputs and outputs
            if(i == row->size() - 2) {
                cout << " ";
            }
        }
        cout << "\n";
    }

}

void print_blif_latch(const BlifLatch* latch) {
    cout << ".latch";
    cout << " " << *latch->input->name;
    cout << " " << *latch->output->name;
    if(latch->type != LatchType::UNSPECIFIED) {
        cout << " " << latch->type;
        cout << " " << *latch->control->name;
    }
    switch(latch->initial_state) {
        case LogicValue::FALSE:
            cout << " 0";
            break;
        case LogicValue::TRUE:
            cout << " 1";
            break;
        case LogicValue::DC:
            cout << " 2";
            break;
        case LogicValue::UNKOWN:
            cout << " 3";
            break;
        default:
            assert(0);
    }
    cout << "\n";

}

void print_blif_subckt(const BlifSubckt* subckt) {
    cout << ".subckt";
    cout << " " << *(subckt->type) << " \\\n";
    for(size_t i = 0; i < subckt->ports.size(); i++) {
        BlifPort* port = subckt->ports[i];
        cout << "\t" << *port->name << "=" << *port->port_conn->net->name; //TODO print net name
        if(i == subckt->ports.size()-1) { 
            cout << " \n";
        } else {
            cout << " \\\n";
        }
    }
}
