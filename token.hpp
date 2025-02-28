#pragma once
#include <string>

enum class TokenType {
    ID, NUM, STRING, 
    PLUS, MINUS, STAR, SLASH, DOUBLESLASH, MOD, POW,
    EQUAL, NOTEQUAL, LESS, GREATER, LESSEQUAL, GREATEREQUAL,
    AND, OR, NOT,
    ASSIGN, PLUSEQUAL, MINUSEQUAL, STAREQUAL, SLASHEQUAL, DOUBLESLASHEQUAL, MODEQUAL, POWEQUAL,
    IN, NOTIN,
    IS, ISNOT, 
    LPAREN, RPAREN, // ()
    LBRACKET, RBRACKET, // []
    LBRACE, RBRACE, // {}
    COMMA,  // , 
    COLON, // :
    DOT,
    INDENT, DEDENT, NEWLINE, END, // 1-2 отступы
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





