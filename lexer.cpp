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
            return extract_identifier();
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

Token Lexer::extract_newline() {
    ++index;
    column = 1;
    ++line;

    return {TokenType::NEWLINE, "\\n", line - 1, column};
}

Token Lexer::extract_indendation() {
    int spaces = 0;
    
    while (index < input.size() && input[index] == ' ') {
        spaces++;
        index++;
        column++;
    }

    if (spaces > indent_level) {
        indent_level = spaces;
        return {TokenType::INDENT, "", line, column};
    }

    if (spaces < indent_level) {
        indent_level = spaces;
        return {TokenType::DEDENT, "", line, column};
    }

    return extract();
}

Token Lexer::extract_identifier() {
    std::size_t size;

    while (std::isalnum(input[index + size]) || input[index + size] == '_') {
        size++;
        column++;
    } 

    std::string name(input, index, size);
    index += size;
    return {TokenType::ID, name};
}