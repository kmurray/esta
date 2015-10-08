%{
#include <vector>
#include <string>
#include <iostream>
#include <cassert>

#include "blif_data.hpp"
#include "BlifParser.hpp"
#include "blif_parse_common.hpp"
#include "BlifParseError.hpp"

using std::vector;
using std::string;
using std::cout;

%}

%union {
    BlifData* blif_data_val;
    BlifModel* blif_model_val;
    BlifNames* blif_names_val;
    BlifLatch* blif_latch_val;
    BlifSubckt* blif_subckt_val;
    PortConnection* port_connection_val;

    std::string* str_val;

    std::vector<LogicValue>* logic_list_val;
    std::vector<std::string*>* str_list_val;
    std::vector<PortConnection*>* port_connection_list_val;

    LatchTypeControl* latch_type_control_val;
    LatchType latch_type_val;
    LogicValue logic_value_val;
}

%{

//Lexer functions
extern int yylex(YYSTYPE* lvalp, YYLTYPE* llocp, void* scanner);
extern char *BlifParse_get_text (void* yyscanner );

//Error handler
void yyerror(YYLTYPE* llocp, BlifParser* context, const char* msg) {
    throw_parse_error(context, msg);
}

//Converts 'lexer_context' (derived from %lex-param below) to the actual
// lexer context object
#define lexer_context parser_context->get_lexer_context()->get_lexer_state()
%}

/* Verbose error reporting */
%error-verbose

/* Track locations */
%locations
/*%defines*/

/* Use prefix in stead of 'yy' for this parser */
%name-prefix "BlifParse_"

/* Re-entrant (i.e. not global vars) */
%define api.pure full

/* Re-entrant lexer needs parser context argument*/
%parse-param {BlifParser* parser_context}

/* Re-entrant lexer needs lexer context argument */
%lex-param {void* lexer_context}

/* Declare constant */
%token DOT_NAMES ".names"
%token DOT_LATCH ".latch"
%token DOT_MODEL ".model"
%token DOT_SUBCKT ".subckt"
%token DOT_INPUTS ".inputs"
%token DOT_OUTPUTS ".outputs"
%token DOT_CLOCK ".clock"
%token DOT_END ".end"
%token DOT_BLACKBOX ".blackbox"
%token LATCH_FE "fe"
%token LATCH_RE "re"
%token LATCH_AH "ah"
%token LATCH_AL "al"
%token LATCH_AS "as"
%token NIL "NIL"
%token LATCH_INIT_2 "2"
%token LATCH_INIT_3 "3"
%token LOGIC_FALSE "0"
%token LOGIC_TRUE "1"
%token LOGIC_DC "-"
%token EOL "end-of-line"

/* Declare variable tokens */
%token <str_val> STRING
%token <str_val> LOGIC_VALUE_STR

/* Declare types */
%type <str_val> id
%type <blif_data_val> blif_data
%type <blif_model_val> model
%type <blif_names_val> names
%type <blif_latch_val> latch
%type <blif_subckt_val> subckt
%type <port_connection_val> port_connection
%type <latch_type_control_val> latch_type_control
%type <latch_type_val> latch_type
%type <str_val> latch_control
%type <logic_value_val> latch_init
%type <str_list_val> id_list
%type <logic_list_val> so_cover_row
%type <port_connection_list_val> port_connection_list

/* Top level rule */
%start blif_data

%%

blif_data
    : { 
        /*
         * We get the (already allocateD) BlifData top level object from the parser context
         */
        $$ = parser_context->get_blif_data(); 
      }
    | blif_data model DOT_END EOL { 
        /*
         * We explicitly end the model when we see '.end' and reduce here
         * this ensures that only one model is active at a time
         */
        $2->ended = true; 
        $$->models.push_back($2); 
      }
    | blif_data EOL { /* Eat stray EOLs at end of file */ }
    ;

model
    : DOT_MODEL id EOL {
        $$ = new BlifModel(); 
        $$->model_name = $2; 
        $$->ended = false;
      }
    | model DOT_INPUTS id_list EOL {
        assert(!$$->ended);
        if($$->inputs.size() != 0) {
            std::string msg = std::string("Duplicate input list ('.inputs') for model '");
            msg += *($$->model_name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }

        $$->inputs = *$3;
        delete $3;
    }
    | model DOT_OUTPUTS id_list EOL {
        assert(!$$->ended);
        if($$->outputs.size() != 0) {
            std::string msg = std::string("Duplicate output list ('.outputs') for model '");
            msg += *($$->model_name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }

        $$->outputs = *$3;
        delete $3;
      }
    | model DOT_CLOCK id_list EOL {
        assert(!$$->ended);
        if($$->clocks.size() != 0) {
            std::string msg = std::string("Duplicate clock list ('.clock') for model '");
            msg += *($$->model_name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }

        $$->clocks = *$3;
        delete $3;
      }
    | model names {
        assert(!$$->ended);
        assert(!$$->blackbox);
        $$->names.push_back($2);
      }
    | model latch {
        assert(!$$->ended);
        assert(!$$->blackbox);
        $$->latches.push_back($2);
      }
    | model subckt {
        assert(!$$->ended);
        assert(!$$->blackbox);
        $$->subckts.push_back($2);
      }
    | model DOT_BLACKBOX EOL {
        assert(!$$->ended);
        $$->blackbox = true;
      }
    ;

names
    : DOT_NAMES id_list EOL { $$ = new BlifNames; $$->ios = *$2; delete $2; }
    | names so_cover_row EOL { $$->cover_rows.push_back($2); }
    ;

so_cover_row
    : { $$ = new vector<LogicValue>(); }
    | so_cover_row LOGIC_FALSE { $$->push_back(LogicValue::FALSE); }
    | so_cover_row LOGIC_TRUE { $$->push_back(LogicValue::TRUE); }
    | so_cover_row LOGIC_DC { $$->push_back(LogicValue::DC); }
    ;

latch
    : DOT_LATCH id id latch_type_control latch_init EOL {
        $$ = new BlifLatch();
        $$->input = $2;
        $$->output = $3;
        $$->type = $4->type;
        $$->control = $4->control;
        $$->initial_state = $5;
      } 
    ;

latch_type_control
    : { 
        /* Default */ 
        $$ = new LatchTypeControl(); 
        $$->type = LatchType::UNSPECIFIED; 
        $$->control = nullptr; 
      }
    | latch_type latch_control { 
        /* Type and control must be specified together */
        $$ = new LatchTypeControl(); 
        $$->type = $1; 
        $$->control = $2;
      }
    ;

    

latch_type
    : LATCH_FE { $$ = LatchType::FALLING_EDGE; }
    | LATCH_RE { $$ = LatchType::RISING_EDGE; }
    | LATCH_AH { $$ = LatchType::ACTIVE_HIGH; }
    | LATCH_AL { $$ = LatchType::ACTIVE_LOW; }
    | LATCH_AS { $$ = LatchType::ASYNCHRONOUS; }
    ;

latch_control
    : id { $$ = $1; }
    | NIL { $$ = nullptr; }
    ;

latch_init
    :     { $$ = LogicValue::UNKOWN; /* default */ }
    | LOGIC_FALSE { $$ = LogicValue::FALSE; }
    | LOGIC_TRUE { $$ = LogicValue::TRUE; }
    | LATCH_INIT_2 { $$ = LogicValue::DC; }
    | LATCH_INIT_3 { $$ = LogicValue::UNKOWN; }
    ;

subckt
    : DOT_SUBCKT id port_connection_list EOL { $$ = new BlifSubckt(); $$->type = $2; $$->port_connections = *$3; delete $3; }
    ;

port_connection_list
    : { $$ = new std::vector<PortConnection*>(); }
    | port_connection_list port_connection { $$->push_back($2); }

port_connection
    : id '=' id { $$ = new PortConnection; $$->port_name = $1; $$->signal_name = $3; }

id_list
    : { $$ = new vector<string*>(); }
    | id_list id    { $$->push_back($2); }
    ;

id: STRING { $$ = $1; }

%%
