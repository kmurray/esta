#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "OptionParser.h"


using std::cout;
using std::cerr;
using std::endl;
using std::string;


std::tuple<std::string,std::vector<std::string> parse_args(int argc, char** argv);

std::tuple<std::string,std::vector<std::string> parse_args(int argc, char** argv) {
    if(argc < 3) {
        std::cout << "Usage: \n";
        std::cout << "\t" << argv[0] << " vcd_file output_name [output_name ...]\n";
        std::exit(1);
    }

    std::string vcd_file = argv[1];

    std::vector<std::string> outputs;
    for(int i = 2; i < argc; i++) {
        outputs.push_back(argv[i]);
    }

    return std::make_pair(vcd_file, outputs)
}

int main(int argc, char** argv) {
    std::string vcd_file;
    std::vector<std::string> output_names;

    std::bind(vcd_file, output_names) = parse_args(argc, argv);


    

}
