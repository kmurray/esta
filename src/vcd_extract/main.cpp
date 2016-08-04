#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <tuple>
#include <cassert>

#include "OptionParser.h"

#include "vcd_parse.hpp"
#include "vcd_extract.hpp"

using std::string;
using std::vector;
using std::tuple;
using std::cout;
using std::endl;

optparse::Values parse_args(int argc, char** argv);
vector<string> load_names(string filename);

int main(int argc, char** argv) {
    auto options = parse_args(argc, argv);

    auto vcd_file = options.get_as<string>("vcd_file");
    auto clock_name = options.get_as<string>("clock");
    auto input_names = load_names(options.get_as<string>("input_names_file"));
    auto output_names = load_names(options.get_as<string>("output_names_file"));
    auto output_dir = options.get_as<string>("output_dir");

    VcdExtractor vcd_extractor_callback(clock_name, input_names, output_names, output_dir);

    if(vcd_file == "-") {
        //Read from stdin
        parse_vcd(std::cin, vcd_extractor_callback);
    } else {
        //Read from file
        parse_vcd(vcd_file, vcd_extractor_callback);
    }
}

optparse::Values parse_args(int argc, char** argv) {
    auto parser = optparse::OptionParser()
                    .description("Process VCD to produce transition data")
                    ;

    parser.add_option("-v", "--vcd_file")
          .help("The VCD file to process")
          ;

    parser.add_option("-c", "--clock")
          .help("The simulation clock")
          ;

    parser.add_option("-i", "--input_names_file")
          .help("The circuit input names file")
          ;

    parser.add_option("-o", "--output_names_file")
          .help("The circuit output names file")
          ;
    parser.add_option("--output_dir")
          .set_default(".")
          .help("The output directory to write results in")
          ;

    return parser.parse_args(argc, argv);
}

vector<string> load_names(string filename) {
    vector<string> names;

    std::ifstream is(filename.c_str());
    assert(is.good());

    std::string line;
    while(std::getline(is, line)) {
        names.push_back(line);
    }

    return names;
}
/*
 *tuple<string,string,vector<string>,vector<string>, string> parse_args(int argc, char** argv) {
 *    if(argc < 8) {
 *        cout << "Usage: \n";
 *        cout << "\t" << argv[0] << " vcd_file -c clock -i input_name [input_name...] -o output_name [output_name ...] [--output_dir directory]\n";
 *        exit(1);
 *    }
 *
 *    string vcd_file = argv[1];
 *    string clock = argv[3];
 *
 *    vector<string> inputs;
 *    vector<string> outputs;
 *    string output_dir = ".";
 *
 *    int state = 0;
 *    for(int i = 4; i < argc; ++i) {
 *        string value = argv[i];
 *
 *        if(value == "-i") state = 1;
 *        else if(value == "-o") state = 2;
 *        else if(value == "--output_dir") state = 3;
 *        else if(state == 1) inputs.push_back(value);
 *        else if(state == 2) outputs.push_back(value);
 *        else if(state == 3) output_dir = value;
 *        else assert(false);
 *    }
 *
 *    return make_tuple(vcd_file, clock, inputs, outputs, output_dir);
 *}
 */

