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
        delete blif_data;
    }

    return 0;
}
