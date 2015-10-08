#pragma once
#include <exception>
#include <string>

class BlifParseError : public std::runtime_error {
    public:
        explicit BlifParseError(int line_num_, const char* near_text_, const std::string& msg)
            : std::runtime_error(std::string(msg.c_str()))
            , line_num(line_num_)
            , near_text(near_text_) {}

        int line_num;
        std::string near_text;
};
