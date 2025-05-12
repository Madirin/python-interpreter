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
    bool at_line_start = true;

    std::vector<int> indent_stack;          
    std::vector<Token> pending_indent_tokens;
    

    static const std::unordered_map<std::string, TokenType> triggers;

    bool match(bool eof, std::size_t size);

    Token extract();
    Token extract_newline();
    Token extract_indentation();
    Token extract_identifier();
    Token extract_number();
    Token extract_string();
    Token extract_operator();
};


/* if x == 10:
    return x    
    TOKEN(, "if") */