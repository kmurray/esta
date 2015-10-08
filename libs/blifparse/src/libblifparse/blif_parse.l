/* recognize tokens from a Berkely Logic Interchange Format (BLIF) file */
%{
/*
 * Include Files
 */
#include <iostream>

//Forward declaration
class BlifParser;

#include "BlifParser.hpp"

#include "blif_parse_common.hpp"
#include "blif_parse.par.hpp"

//TODO: needted for locations?
#define YY_USER_ACTION yylloc->first_line = yylineno;

//Make flex read from a C++ stream
/*
 *#define YY_INPUT(buf,result,max_size)           \
 *   {                                            \
 *      char c;                                   \
 *      std::istream& is = yyextra->get_stream(); \
 *      is >> c;                                  \
 *      if(is.eof())                              \
 *         result = YY_NULL;                      \
 *      else {                                    \
 *         buf[0] = c;                            \
 *         result = 1;                            \
 *      }                                         \
 *   }
 */
%}

/*
 * Options
 */
/* track line numbers*/
%option yylineno 

/* No lexing accross files */
%option noyywrap

/* unistd.h doesn't exist on windows */
%option nounistd

/* isatty() doesn't exist on windows */
%option never-interactive

/* Re-entrant (i.e. no global vars) */
%option reentrant

/* User supplied extra parser argument */
%option extra-type="BlifParser*"

/* Use prefix instead of 'yy' before functions */
%option prefix="BlifParse_"

/* Match bison calling conventions */
%option bison-bridge

/* Track locations for bison error reporting */
%option bison-locations

/* Common character classes */
ALPHA_SYMBOLS [-a-zA-Z_~|:*/\[\]\.\{\}^+$]
DIGITS [0-9]
ALPHA_NUM_SYMBOLS ({ALPHA_SYMBOLS}|{DIGITS})
BACK_SLASH [\\]
WS [ \t]
ENDL (\n|\n\r)

/* Special Parser States */
%x LATCH
%x NAMES
%x SO_COVER

/*
 * Symbol Definitions
 */
%%
<*>#.*{ENDL}                    { /* ignore comments */ }
^{WS}*{ENDL}                    { /* Ignore blank lines. */ }
\\{ENDL}{WS}*{ENDL}             { 
                                  /* 
                                   * Do forward end of line if the last line was a continuation. 
                                   *
                                   * Some times line continuations are followed by blank lines (which 
                                   * are otherwise ignored). In these cases we *do* want to 
                                   * forward EOL, so the parser knows the continued line has finished 
                                   */ 
                                  return EOL; 
                                }
<*>\\{ENDL}                        { /* line continuation (don't forward EOL to parser) */ }
{ENDL}                          { return EOL; }
<*>{WS}+                           { /* skip white space */ }

<*>\.names                         { 
                                  /*
                                   * To process the single output cover rows of the names directly as symbols
                                   * (rather than as strings) we use a special lexer state.
                                   */
                                  BEGIN(NAMES);
                                  return DOT_NAMES; 
                                }
<*>\.latch                         { 
                                  /*
                                   * The initial state value of a latch is ambiguous (it chould be 
                                   * interpreted as a string or logic value string). So we use
                                   * a special lexer state to capture it.
                                   */
                                  BEGIN(LATCH); 
                                  return DOT_LATCH; 
                                }
<*>\.model                      { BEGIN(INITIAL); return DOT_MODEL; }
<*>\.subckt                     { BEGIN(INITIAL); return DOT_SUBCKT; }
<*>\.inputs                     { BEGIN(INITIAL); return DOT_INPUTS; }
<*>\.outputs                    { BEGIN(INITIAL); return DOT_OUTPUTS; }
<*>\.end                        { BEGIN(INITIAL); return DOT_END; }
<*>\.blackbox                   { BEGIN(INITIAL); return DOT_BLACKBOX; }
[-01]+                          { 
                                  /*
                                   * Request strings through the parser object (passed as yyextra).
                                   * This allows us to uniquify strings and prevent duplicates.
                                   * This can save a substantial amount of memory as many strings
                                   * (e.g. nets, subckt types and ports) are commonly duplicated.
                                   */
                                  yylval->str_val = yyextra->get_str_table()->make_str(yytext); 
                                  return LOGIC_VALUE_STR; 
                                }
<LATCH>fe                       { return LATCH_FE; }
<LATCH>re                       { return LATCH_RE; }
<LATCH>ah                       { return LATCH_AH; }
<LATCH>al                       { return LATCH_AL; }
<LATCH>as                       { return LATCH_AS; }
<LATCH>NIL                      { return NIL; }
<LATCH>0                        { return LOGIC_FALSE; }
<LATCH>1                        { return LOGIC_TRUE; }
<LATCH>2                        { return LATCH_INIT_2; }
<LATCH>3                        { return LATCH_INIT_3; }
<LATCH>{ENDL}                   { 
                                  /*
                                   * Latches are only every defined on a single line,
                                   * so when we see the end of a line while in the LATCH
                                   * state we can go back to the regular (INITIAL) state.
                                   */
                                  BEGIN(INITIAL); return EOL; 
                                }
<SO_COVER>0                     { return LOGIC_FALSE; }
<SO_COVER>1                     { return LOGIC_TRUE; }
<SO_COVER>\-                    { return LOGIC_DC; }
<SO_COVER>{ENDL}                { return EOL; }
<NAMES>{ENDL}                   { 
                                  /*
                                   * Once we reach the end of a line in NAMES state (i.e. the end of a .names line)
                                   * we expect the truth table (in single output cover format) to follow, so we enter
                                   * the SO_COVER state.
                                   */
                                  BEGIN(SO_COVER); return EOL; }
<INITIAL,NAMES,LATCH>({DIGITS}({ALPHA_NUM_SYMBOLS}|{BACK_SLASH})*{ALPHA_NUM_SYMBOLS}+)|(({ALPHA_SYMBOLS}|{BACK_SLASH})*({ALPHA_NUM_SYMBOLS}|{BACK_SLASH})*{ALPHA_NUM_SYMBOLS}) {
                                    /* The terrible regex above covers two cases:
                                     *  1) An identifier starts with a digit, contains a combination of
                                     *     alpha numeric and symbolic characters. To avoid conflicts with
                                     *     line continuation, back slashes are forbidden in the last character.
                                     *  2) An identifier with a non-numeric first letter (i.e. alpha, symbol, back slash)
                                     *     followed by any number of (alpha, num, symbol, back slash) the last character
                                     *     can be any alpha/num/symbol excluding back slashes (to avoid conflicts with line continuation)
                                     *
                                     */
                                    /*
                                     * We allow all sorts of characters in regular strings.
                                     * However we need to be careful about line continuations
                                     * in particular, it is possible that we could have a string 
                                     * followed by a continuation with no space for this reason, 
                                     * we do not allow a continuation (backslash, \\ in escaped 
                                     * form in the regex) in the last character of the string.
                                     */
                                    yylval->str_val = yyextra->get_str_table()->make_str(yytext); 
                                    /*std::cout << "STRING: " << *(yylval->str_val) << "\n";*/
                                    return STRING; 
                                }
.                               { /* Literal character to pass unrecognized stuff to parser
                                     (i.e. brackets) */
                                  /*std::cout << "LITERAL: " << yytext << "\n";*/
                                  return (int)(*yytext); 
                                }
<<EOF>>                         { /* If the file has no blank line at the end there will
                                     not be the expected EOL following the last command. 
                                     So first time through, return EOL, and subsequently 
                                     return 0 (which indicated end of file). This ensures
                                     there will always be an EOL provided to the parser. 
                                     However it may also generate a stray EOL if the last
                                     line IS blank - so the parse must handle those correclty. */
                                  static int once = 0; return (once = !once) ? EOL : 0; 
                                }
%%

