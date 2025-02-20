#include "lexer.hpp"
#include <stdexcept>

Lexer::Lexer(const std::string& input) : input(input) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (index < input.size()) {
        tokens.push_back(extract());
    }

    return tokens;
}

Token Lexer::extract() {
    while (index < input.size()) {
        while (std::isspace(input[index])) {
            ++index;
            ++column;
        }
        if (input[index] == '\n') {
            return extract_newline(); 
        }

        if (column = 1) {
            return extract_indendation();
        }

        if (std::isalpha(input[index]) || input[index] == '_') {
            return extract_indendation();
        }

        if (std::isdigit(input[index])) {
            return extract_number();
        }

        if (input[index] == '"') {
            return extract_string();
        }

        if (std::ispunct(input[index])) {
            return extract_operator();
        }

        throw std::runtime_error("Unexpected symbol " + input[index]);
    }
    
    return {TokenType::END, ""};
}