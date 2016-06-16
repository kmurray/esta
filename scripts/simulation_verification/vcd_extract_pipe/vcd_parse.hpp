#pragma once
#include <string>
#include <iosfwd>

class VcdCallback {
    
    public:
        virtual void start() {}
        virtual void finish() {}
        virtual void add_var(std::string type, size_t width, std::string id, std::string name) {}
        virtual void set_time(size_t time) {}
        virtual void add_transition(std::string id, char val) {}
};

void parse_vcd(std::string filename, VcdCallback& callback);
void parse_vcd(std::istream& is, VcdCallback& callback);
