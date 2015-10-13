#include "BlifLexerContext.hpp"

#include "blif_data.hpp"
#include "blif_parse.par.hpp"
#include "blif_parse.lex.hpp"

void BlifLexerContext::set_infile(FILE* file) {
    BlifParse_set_in(file, this->lexer_state_);
}

void BlifLexerContext::init_lexer(BlifParser* parser) {
    BlifParse_lex_init(&this->lexer_state_);
    BlifParse_lex_init_extra(parser, &this->lexer_state_);
}

void BlifLexerContext::destroy_lexer() {
    //There seems to be memory in flex which this doesn't
    //free... not much we can do
    BlifParse_lex_destroy(this->lexer_state_);
}


