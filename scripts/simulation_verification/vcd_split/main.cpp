#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#include <cmath>
#include <tuple>
#include <vector>

using std::cout;
using std::endl;

std::tuple<std::string,size_t> parse_args(int argc, char** argv);

int main(int argc, char** argv) {
    size_t n;
    std::string vcd_file;
    tie(vcd_file, n) = parse_args(argc, argv);

    cout << "Counting lines" << endl;

    std::ifstream vcd_is(vcd_file);

    size_t line_num = 0;
    size_t header_end_line = 0;
    std::vector<size_t> time_step_lines;

    //Fist pass count lines and not good break positions
    std::string line;
    bool in_header = true;
    bool seen_dumpvars = false;
    while(std::getline(vcd_is, line)) {
        if(in_header) {
            if(line == "$dumpvars") {
                seen_dumpvars = true;
            }

            if(line == "$end" and seen_dumpvars) {
                in_header = false;
                header_end_line = line_num;
            }
        } else if (line[0] == '#') {
            time_step_lines.push_back(line_num);
        }

        line_num++;
    }

    size_t num_lines = line_num;

    cout << "Num Lines: " << num_lines << endl;
    cout << "Header end line: " << header_end_line << endl;
    cout << "Num timesteps: " << time_step_lines.size() << endl;

    size_t time_steps_per_file = ceil((float) time_step_lines.size() / (n-1)); //-1 since we put the header in a separate file
    cout << "time steps per file: " << time_steps_per_file << endl;

    //Calculate the breakpoints
    std::vector<size_t> breaks = {0, header_end_line + 1};

    size_t i = time_steps_per_file;
    while(*(--breaks.end()) < num_lines && i < time_step_lines.size()) {
        size_t next_break = std::min(num_lines, time_step_lines[i]);
        breaks.push_back(next_break);
        i += time_steps_per_file;
    }
    breaks.push_back(num_lines);

    cout << "{";
    for(size_t break_line : breaks) {
        cout << break_line << ", ";
    }
    cout << "}" << endl;

    std::string base_name;
    auto iter = vcd_file.find_last_of("/");
    if(iter != std::string::npos) {
        base_name = std::string(vcd_file.begin() + iter+1, vcd_file.end());
    } else {
        base_name = vcd_file;        
    }

    cout << "Base name: " << base_name << endl;

    //Write the split files
    i = 0;
    line_num = 0;

    //Restart the input file
    vcd_is.close();
    vcd_is.open(vcd_file);
    std::string split_filename = base_name + "." + std::to_string(i);
    std::ofstream outfile(split_filename);
    cout << "Writting " << split_filename << endl;
    while(std::getline(vcd_is, line)) {

        if(line_num == breaks[i+1]) {
            i++;
            split_filename = base_name + "." + std::to_string(i);
            outfile.close();
            outfile.open(split_filename);
            cout << "Writting " << split_filename << endl;
        }

        outfile << line << "\n";
        line_num++;
    }

    return 0;
}

std::tuple<std::string,size_t> parse_args(int argc, char** argv) {
    if(argc != 3) {
        cout << "Usage: \n";
        cout << "\t" << argv[0] << " vcd_file num_output_files\n";
        exit(1);
    }

    std::string vcd_file = argv[1];
    std::stringstream ss;
    ss << argv[2];
    size_t n;
    ss >> n;

    return make_tuple(vcd_file, n);
}
