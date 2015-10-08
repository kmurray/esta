#pragma once
#include <cstdio>

//Forward Declaration
class BlifParser;


class BlifLexerContext {
    public:

        BlifLexerContext(BlifParser* parser) { init_lexer(parser); }
        virtual ~BlifLexerContext() { destroy_lexer(); }

        void set_infile(FILE* file);

        void* get_lexer_state() { return lexer_state_; }

    protected:
        void init_lexer(BlifParser* parser);
        void destroy_lexer();
    private:
        void* lexer_state_;
};

