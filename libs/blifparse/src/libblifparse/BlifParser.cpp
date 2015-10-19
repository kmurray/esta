#include <iostream>

#include "BlifParser.hpp"

#include "blif_parse.par.hpp"
#include "blif_parse.lex.hpp"

BlifData* BlifParser::parse(std::string filename) {
    //Allocate a new blif data structure to hold the file results
    blif_data_ = new BlifData();

    //Open the file
    FILE* blif_file = fopen(filename.c_str(), "r");
    if(blif_file == nullptr) {
        assert(0);
    } else {
        //Direct the lexer to the file
        lexer_context_.set_infile(blif_file);

        //Parse the file
        BlifParse_parse(this);

        fclose(blif_file);
    }

    //Resolve internal references to nets
    blif_data_->resolve_nets();

    if(clean_nets_) {
        blif_data_->clean_nets();
    }

    if(sweep_dangling_ios_) {
        blif_data_->sweep_dangling_ios(blif_data_->get_top_module());
    }

    return blif_data_;
}

