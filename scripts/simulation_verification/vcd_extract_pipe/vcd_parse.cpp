#include <fstream>
#include <sstream>
#include <cassert>
#include <vector>

#include "gzstream.h"

#include "vcd_parse.hpp"
#include "util.hpp"

using std::string;


void parse_date(string line, VcdCallback& callback);
void parse_version(string line, VcdCallback& callback);
void parse_timescale(string line, VcdCallback& callback);
void parse_definitions(string line, VcdCallback& callback);
void parse_timevalues(string line, VcdCallback& callback);

void parse_vcd(string filename, VcdCallback& callback) {
    
    if(filename.size() > 3 &&
       filename[filename.size()-3] == '.' &&
       filename[filename.size()-2] == 'g' &&
       filename[filename.size()-1] == 'z') {
        //The file appears gzipped
        igzstream is(filename.c_str());
        assert(is.good());

        parse_vcd(is, callback);
    } else {
        //Regular file
        std::ifstream is(filename);
        assert(is.good());

        parse_vcd(is, callback);
    }
}

enum class ParseState {
    INITIAL,
    DATE,
    VERSION,
    TIMESCALE,
    DEFINITIONS,
    TIMEVALUES,
};

void parse_vcd(std::istream& is, VcdCallback& callback) {
    callback.start();
    
    string line;

    ParseState state = ParseState::INITIAL;

    while(std::getline(is, line)) {
        //State transitions
        if(line == "$date") {
            state = ParseState::DATE;
            continue;
        } else if (line == "$version") {
            state = ParseState::VERSION;
            continue;
        } else if (line == "$timescale") {
            state = ParseState::TIMESCALE;
            continue;
        } else if (line.size() > 0 && line.find("$scope") == 0) {
            state = ParseState::DEFINITIONS;
            continue;
        } else if (line == "$enddefinitions $end") {
            state = ParseState::TIMEVALUES;
            continue;
        } else {
            //pass
        }

        //Actions
        if(state == ParseState::DATE) {
            parse_date(line, callback);
        } else if (state == ParseState::VERSION) {
            parse_version(line, callback);
        } else if (state == ParseState::TIMESCALE) {
            parse_timescale(line, callback);
        } else if (state == ParseState::DEFINITIONS) {
            parse_definitions(line, callback);
        } else if (state == ParseState::TIMEVALUES) {
            parse_timevalues(line, callback);
        } else {
            assert(false && "Invalid state!");
        }
    }

    callback.finish();
}

void parse_date(string line, VcdCallback& callback) {
    //pass
}

void parse_version(string line, VcdCallback& callback) {
    //pass
}

void parse_timescale(string line, VcdCallback& callback) {
    //pass
}

void parse_definitions(string line, VcdCallback& callback) {
    if(line.empty()) return;

    std::vector<string> tokens = split(line, ' ');
    if(tokens[0] == "$scope") {
        //pass
    } else if (tokens[0] == "$var") {
        std::string type = tokens[1];
        size_t width;
        std::stringstream ss(tokens[2]);
        ss >> width;
        std::string id = tokens[3];
        std::string name = tokens[4];
        assert(tokens[5] == "$end" && "Expected $end");

        callback.add_var(type, width, id, name);
    } else if (tokens[0] == "$upscope") {
        //pass
    } else {
        assert(false && "Unexpected starting token in definitions");
    }
}

void parse_timevalues(string line, VcdCallback& callback) {
    if (line[0] == '#') {
        size_t time;
        std::stringstream ss(string(line.begin()+1,line.end()));
        ss >> time;
        callback.set_time(time);
    } else if(line == "$dumpvars") {
        //pass
    } else if(line == "$end") {
        //pass
    } else {
        assert(line[0] == '0' || line[0] == '1' || line[0] == 'x');

        std::string id(line.begin()+1, line.end());
        char val = line[0];

        callback.add_transition(id, val);
    }
}


