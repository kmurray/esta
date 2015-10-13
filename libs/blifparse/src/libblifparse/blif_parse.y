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

void add_port_conn(BlifParser* context, BlifPort* port, std::string* net_name);
%}

%union {
    BlifData* blif_data_val;
    BlifModel* blif_model_val;
    BlifNames* blif_names_val;
    BlifLatch* blif_latch_val;
    BlifSubckt* blif_subckt_val;

    std::string* str_val;

    std::vector<LogicValue>* logic_list_val;
    std::vector<std::string*>* str_list_val;

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
%type <latch_type_control_val> latch_type_control
%type <latch_type_val> latch_type
%type <str_val> latch_control
%type <logic_value_val> latch_init
%type <str_list_val> id_list
%type <logic_list_val> so_cover_row

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
        $$ = new BlifModel($2); 
      }
    | model DOT_INPUTS id_list EOL {
        assert(!$$->ended);
        if($$->inputs.size() != 0) {
            std::string msg = std::string("Duplicate input list ('.inputs') for model '");
            msg += *($$->name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }

        for(auto port_name : *$3) {
            //Create the port
            BlifPort* port = new BlifPort(port_name, $$);

            //Add port -> net connections
            //Models generate nets with the same name as ports
            add_port_conn(parser_context, port, port_name);

            //Add the connected port to the model
            $$->inputs.push_back(port);
        }

        delete $3;
    }
    | model DOT_OUTPUTS id_list EOL {
        assert(!$$->ended);
        if($$->outputs.size() != 0) {
            std::string msg = std::string("Duplicate output list ('.outputs') for model '");
            msg += *($$->name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }
        for(auto port_name : *$3) {
            //Create the port
            BlifPort* port = new BlifPort(port_name, $$);

            //Add port -> net connections
            //Models generate nets with the same name as ports
            add_port_conn(parser_context, port, port_name);

            //Add the connected port to the model
            $$->outputs.push_back(port);
        }

        delete $3;
      }
    | model DOT_CLOCK id_list EOL {
        assert(!$$->ended);
        if($$->clocks.size() != 0) {
            std::string msg = std::string("Duplicate clock list ('.clock') for model '");
            msg += *($$->name);
            msg += std::string("'");
            throw_parse_error(parser_context, msg);
        }
        for(auto port_name : *$3) {
            //Create the port
            BlifPort* port = new BlifPort(port_name, $$);

            //Add port -> net connections
            //Models generate nets with the same name as ports
            add_port_conn(parser_context, port, port_name);

            //Add the connected port to the model
            $$->clocks.push_back(port);
        }
        delete $3;
      }
    | model names {
        assert(!$$->ended);
        assert(!$$->blackbox);
        $$->names.push_back($2);
      }
    | model latch EOL {
        assert(!$$->ended);
        assert(!$$->blackbox);
        $$->latches.push_back($2);
      }
    | model subckt EOL {
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
    : DOT_NAMES id_list EOL { 
        $$ = new BlifNames();
        for(size_t i = 0; i < $2->size(); i++) {
            auto port_name = (*$2)[i];

            //Create the port
            BlifPort* port = new BlifPort(port_name, $$);

            //Add port -> net connections
            //Names generate nets with the same name as ports
            add_port_conn(parser_context, port, port_name);

            //Add the connected port to the model
            $$->ports.push_back(port);
        }

        delete $2;
      }
    | names so_cover_row EOL { $$->cover_rows.push_back($2); }
    ;

so_cover_row
    : { $$ = new vector<LogicValue>(); }
    | so_cover_row LOGIC_FALSE { $$->push_back(LogicValue::FALSE); }
    | so_cover_row LOGIC_TRUE { $$->push_back(LogicValue::TRUE); }
    | so_cover_row LOGIC_DC { $$->push_back(LogicValue::DC); }
    ;

latch
    : DOT_LATCH id id latch_type_control latch_init {
        $$ = new BlifLatch();

        //Add the connected ports to the latch
        $$->input = new BlifPort($2, $$);
        $$->output = new BlifPort($3, $$);
        $$->control = new BlifPort($4->control, $$);

        //Add port -> net connections
        //Latches use net names the same as ports
        add_port_conn(parser_context, $$->input, $2);
        add_port_conn(parser_context, $$->output, $3);
        add_port_conn(parser_context, $$->control, $4->control);

        $$->type = $4->type;
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
    : DOT_SUBCKT id {
        $$ = new BlifSubckt($2);
      }
    | subckt id '=' id {
        BlifPort* port = new BlifPort($2, $$);

        add_port_conn(parser_context, port, $4);

        $$->ports.push_back(port); 
      }
    ;

/*
 *port_connection_list
 *    : { $$ = new std::vector<PortConnection*>(); }
 *    | port_connection_list port_connection { $$->push_back($2); }
 *
 *port_connection
 *    : id '=' id { $$ = new PortConnection; $$->port_name = $1; $$->signal_name = $3; }
 */

id_list
    : id { $$ = new vector<string*>(); $$->push_back($1); }
    | id_list id { $1->push_back($2); $$ = $1; }
    ;

id: STRING { $$ = $1; }

%%

void add_port_conn(BlifParser* parser_context, BlifPort* port, std::string* net_name) {

    assert(port->port_conn == nullptr);

    if(net_name != nullptr) {
        BlifData* blif_data = parser_context->get_blif_data();

        //Get/create the associated net
        // Models create nets with the names of their ports
        BlifNet* net = blif_data->get_net(net_name);

        //Create the connection between net and port
        BlifPortConn* conn = new BlifPortConn(port, net);

        //Add connection to port
        port->port_conn = conn; 
    }
}
