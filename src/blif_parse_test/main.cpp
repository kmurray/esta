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
        cout << "Parsing file: " << argv[i] << endl;

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
            /*
             *for(const BlifModel* model : blif_data->models) {
             *    print_blif_model(model);
             *    cout << "\n";
             *}
             *cout << endl;
             */
            cout << "\tOK" << endl;
            delete blif_data;
        }
    }

    return 0;
}

void print_blif_model(const BlifModel* model) {
    cout << ".model " << *(model->model_name) << "\n";

    if(model->inputs.size() > 0) {
        cout << ".inputs";
        for(string* input : model->inputs) {
            cout << " " << *input;
        }
        cout << "\n";
    }

    if(model->outputs.size() > 0) {
        cout << ".outputs";
        for(string* output : model->outputs) {
            cout << " " << *output;
        }
        cout << "\n";
    }

    if(model->clocks.size() > 0) {
        cout << ".clocks";
        for(string* clock : model->clocks) {
            cout << " " << *clock;
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
    for(const string* io : names->ios) {
        cout << " " << *io;
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
    cout << " " << *latch->input;
    cout << " " << *latch->output;
    if(latch->type != LatchType::UNSPECIFIED) {
        cout << " " << latch->type;
        cout << " " << *latch->control;
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
    for(size_t i = 0; i < subckt->port_connections.size(); i++) {
        PortConnection* pc = subckt->port_connections[i];
        cout << "\t" << *(pc->port_name) << "=" << *(pc->signal_name);
        if(i == subckt->port_connections.size()-1) { 
            cout << " \n";
        } else {
            cout << " \\\n";
        }
    }
}
