#pragma once
#include "token.hpp"

#include <string>
#include <vector>
#include <unordered_map>

class Lexer {
public:
    Lexer(const std::string&);
    std::vector<Token> tokenize();
private:
    std::string input;
    std::size_t index = 0;
    int line = 1;
    int column = 1;
    int indent_level = 0;

    static const std::unordered_map<std::string, TokenType> triggers;

    Token extract();
    Token extract_newline();
    Token extract_indendation();
    Token extract_identifier();
    Token extract_number();
    Token extract_string();
    Token extract_operator();
};


/* if x == 10:
    return x    
    TOKEN(, "if") */