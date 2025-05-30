#pragma once
#include <string>

enum class TokenType {
    // keywords
    ID, INTNUM, FLOATNUM, STRING, BOOL, NONE,// floatnum(literal) intnum(literal), bool, none
    PLUS, MINUS, STAR, SLASH, DOUBLESLASH, MOD, POW,
    EQUAL, NOTEQUAL, LESS, GREATER, LESSEQUAL, GREATEREQUAL,
    AND, OR, NOT,
    IF, ELSE, ELIF,
    WHILE, FOR, BREAK, PASS, CONTINUE, DEF, RETURN, 
    EXIT, PRINT, INPUT, ASSERT,
    ASSIGN, PLUSEQUAL, MINUSEQUAL, STAREQUAL, SLASHEQUAL, DOUBLESLASHEQUAL, MODEQUAL, POWEQUAL,
    CLASS,
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





