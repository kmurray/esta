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
    Cudd mgr();
    //DdManager* mgr = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0);

    BlifModel* model = blif_data->models[0];
    
    BlifNames* names = model->names[0];
    for(size_t i = 0; i < names->ios.size() - 1; i--) {
        cout << *names->ios[i] << " ";
    }
    cout << "\n";
    for(auto row_ptr : names->cover_rows) {
        for(auto logic_val : *row_ptr) {
            cout << logic_val << " ";
        }
        cout << "\n";
    }

}
