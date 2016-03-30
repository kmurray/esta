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

struct Options {
    bool print = false;
    bool clean_nets = true;
    bool sweep_ios = true;
};

bool process_option(Options& options, const std::string option);

void print_blif_net_sizes(const BlifModel* blif_model, std::string prefix);
//void print_blif_net_details(const BlifModel* blif_model, std::string prefix);
void print_blif(const BlifData* blif_data);

void print_blif_model(const BlifModel* model);
void print_blif_names(const BlifNames* names);
void print_blif_latch(const BlifLatch* latch);
void print_blif_subckt(const BlifSubckt* subckt);



int main(int argc, char** argv) {
    if(argc < 2) {
        cout << "Usage: " << argv[0] << " [-print | -no_print*] [-clean_nets* | -no_clean_nets] [-sweep_ios* | -no_sweep_ios] filename1.blif [filename2.blif [filename3.blif [...]]]" << endl;
        cout << "Defaults marked with '*'" << endl;
        return 1;
    }

    Options options;

    int i = 1;
    while(process_option(options, argv[1])) {
        i++;
    }

    for(; i < argc; i++) {
        cerr << "Parsing file: " << argv[i] << endl;

        //Create the parser
        BlifParser parser;
        parser.set_clean_nets(true)
              .set_sweep_dangling_ios(true);

        //Load the file
        BlifData* blif_data = nullptr;
        try {
            blif_data = parser.parse(argv[i]);;
        } catch (BlifParseLocationError& e) {
            cerr << argv[i] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
            return 1;
        } catch (BlifParseError& e) {
            cerr << argv[i] << ":" << " " << e.what() << endl; 
            return 1;
        }

        if(blif_data != nullptr) {
            //Walk and print the blif
            if(options.print) {
                print_blif(blif_data);
            }
            cerr << "\tOK" << endl;
            delete blif_data;
        } else {
            cerr << "\tFailed to parse " << argv[i] << endl;
            return 1;
        }
    }

    return 0;
}

bool process_option(Options& options, const std::string option) {
    if(option == "-print") {
        options.print = true;
        return true;
    } else if (option == "-no_print") {
        options.print = false;
        return true;
    } else if (option == "-clean_nets") {
        options.clean_nets = true;
        return true;
    } else if (option == "-no_clean_nets") {
        options.clean_nets = false;
        return true;
    } else if (option == "-sweep_ios") {
        options.sweep_ios = true;
        return true;
    } else if (option == "-no_sweep_ios") {
        options.sweep_ios = false;
        return true;
    }
    return false;
}

void print_blif_net_sizes(const BlifModel* blif_model, std::string prefix) {
    for(BlifNet* net : blif_model->nets) {
        cout << prefix;
        cout << "Net: '" << *net->name << "'";
        cout << " Drivers: " << net->drivers.size();
        cout << " Sinks: " << net->sinks.size();
        cout << "\n";
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

    if(model->nets.size() > 0) {
        print_blif_net_sizes(model, "#");
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
