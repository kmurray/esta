#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>

#include "blif_parse.hpp"

#include "cudd.h"
#include "cuddObj.hh"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

void build_bdd(BlifData* blif_data);

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cout << "Usage: " << argv[0] << " filename1.blif" << endl;
        return 1;
    }

    cout << "Parsing file: " << argv[1] << endl;

    //Create the parser
    BlifParser parser;

    //Load the file
    BlifData* blif_data = nullptr;
    try {
        blif_data = parser.parse(argv[1]);;
    } catch (BlifParseError& e) {
        cerr << argv[1] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
        return 1;
    }

    if(blif_data != nullptr) {
        cout << "\tOK" << endl;

        cout << "Building BDD..." << endl;
        build_bdd(blif_data);
        delete blif_data;
    }

    return 0;
}


void build_bdd(BlifData* blif_data) {
    Cudd mgr;

    BDD f = mgr.bddZero();

    BlifModel* model = blif_data->models[0];
    
    BlifNames* names = model->names[0];
    for(auto row_ptr : names->cover_rows) {
        BDD cube = mgr.bddOne();

        //Loop through the input plane
        for(size_t i = 0; i < row_ptr->size()-1; i++) {
            BDD var = mgr.bddVar(i);

            LogicValue val = (*row_ptr)[i];
            cout << val << " ";

            switch(val) {
                case LogicValue::TRUE:
                    cube &= var;
                    break;
                case LogicValue::FALSE:
                    cube &= !var;
                    break;
                default:
                    assert(0);
                    break;
            }
        }

        //Last element is the output value
        LogicValue output_val = (*row_ptr)[row_ptr->size()-1];

        cout << "| " << output_val << "\n";

        //Only expect ON set cover
        assert(output_val == LogicValue::TRUE);

        cout << "Cube: " << cube << "\n";

        f |= cube;
    }

    //Show the BDD
    cout << "f: " << f << "\n"; 

}
