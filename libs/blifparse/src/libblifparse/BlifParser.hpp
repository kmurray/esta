#pragma once
#include <cstdio>
#include <cassert>
#include <string>

#include "BlifLexerContext.hpp"
#include "blif_data.hpp"

//Forward Declaration
class StringTable;

class BlifParser {
    public:
        BlifParser()
            : lexer_context_(this)
            , blif_data_(nullptr) {
        }
        virtual ~BlifParser() { }

        BlifData* parse(std::string filename);

        std::string* make_str(char* str);

        BlifData* get_blif_data() { return blif_data_; }
        BlifLexerContext* get_lexer_context() { return &lexer_context_; }
        StringTable* get_str_table() { assert(blif_data_ != nullptr); return &blif_data_->str_table; }
    private:
        BlifLexerContext lexer_context_;
        BlifData* blif_data_;
};

