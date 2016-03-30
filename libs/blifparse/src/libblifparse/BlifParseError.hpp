#pragma once
#include <stdexcept>
#include <string>

class BlifParseError : public std::runtime_error {
    public:
        explicit BlifParseError(const std::string& msg)
            : std::runtime_error(msg)
            {}
};

class BlifParseLocationError : public BlifParseError {
    public:
        explicit BlifParseLocationError(int line_num_, const std::string& near_text_, const std::string& msg)
            : BlifParseError(msg)
            , line_num(line_num_)
            , near_text(near_text_)
        {}

        int line_num;
        std::string near_text;
};
