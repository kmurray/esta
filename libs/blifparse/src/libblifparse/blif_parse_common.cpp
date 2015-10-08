#include <iostream>

#include "blif_parse_common.hpp"
#include "blif_data.hpp"
#include "BlifParseError.hpp"

#include "blif_parse.par.hpp"
#include "blif_parse.lex.hpp"

void throw_parse_error(BlifParser* context, const std::string& msg) {
    void* scanner_context = context->get_lexer_context()->get_lexer_state();
    int line_num = BlifParse_get_lloc(scanner_context)->first_line;
    const char* near_text = BlifParse_get_text(scanner_context);

    throw BlifParseError(line_num, near_text, msg);
}
