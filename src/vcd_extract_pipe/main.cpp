#include <string>
#include <vector>
#include <iostream>
#include <tuple>
#include <cassert>

#include "vcd_parse.hpp"
#include "vcd_extract.hpp"

using std::string;
using std::vector;
using std::tuple;
using std::cout;
using std::endl;

tuple<string,string,vector<string>,vector<string>,string> parse_args(int argc, char** argv);

int main(int argc, char** argv) {
    string vcd_file;
    string clock_name;
    vector<string> input_names;
    vector<string> output_names;
    string output_dir;

    tie(vcd_file, clock_name, input_names, output_names, output_dir) = parse_args(argc, argv);

    VcdExtractor vcd_extractor_callback(clock_name, input_names, output_names, output_dir);

    if(vcd_file == "-") {
        //Read from stdin
        parse_vcd(std::cin, vcd_extractor_callback);
    } else {
        //Read from file
        parse_vcd(vcd_file, vcd_extractor_callback);
    }
}

tuple<string,string,vector<string>,vector<string>, string> parse_args(int argc, char** argv) {
    if(argc < 8) {
        cout << "Usage: \n";
        cout << "\t" << argv[0] << " vcd_file -c clock -i input_name [input_name...] -o output_name [output_name ...] [--output_dir directory]\n";
        exit(1);
    }

    string vcd_file = argv[1];
    string clock = argv[3];

    vector<string> inputs;
    vector<string> outputs;
    string output_dir = ".";

    int state = 0;
    for(int i = 4; i < argc; ++i) {
        string value = argv[i];

        if(value == "-i") state = 1;
        else if(value == "-o") state = 2;
        else if(value == "--output_dir") state = 3;
        else if(state == 1) inputs.push_back(value);
        else if(state == 2) outputs.push_back(value);
        else if(state == 3) output_dir = value;
        else assert(false);
    }

    return make_tuple(vcd_file, clock, inputs, outputs, output_dir);
}

