#pragma once
#include <string>

enum class TokenType {
    ID, NUM, STRING, KEYWORD,
    PLUS, MINUS, STAR, SLASH, DOUBLESLASH, MOD, POW,
    EQUAL, NOTEQUAL, LESS, GREATER, LESSEQUAL, GREATEREQUAL,
    AND, OR, NOT,
    ASSIGN, PLUSEQUAL, MINUSEQUAL, STAREQUAL, SLASHEQUAL, DOUBLESLASHEQUAL, MODEQUAL, POWEQUAL,
    IN, NOTIN,
    IS, ISNOT, 
    LPAREN, RPAREN, // ()
    LBRACKET, RBRACKET, // []
    LBRACE, RBRACE, // {}
    COMMA, SEMICOLON, ARROW, // , ; ->
    COLON, // :
    DOT,
    INDENT, DEDENT, NEWLINE, COMMENT, END, // 1-2 отступы
    AT, ELLIPSIS // @, ...
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    bool operator==(TokenType type) const {
        return this->type == type;
    }
    
};





