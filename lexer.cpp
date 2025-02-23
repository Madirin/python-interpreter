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
    std::size_t size = 0;
    
    while (index + size < input.size() && 
           (std::isalnum(input[index + size]) || input[index + size] == '_')) {
        size++;
        column++;
    }
    
    std::string name(input, index, size);
    index += size;
    
    return {TokenType::ID, name, line, column};
}













const std::unordered_map<std::string, TokenType> Lexer::triggers = {
    {"if", TokenType::ID},
    {"else", TokenType::ID},
    {"elif", TokenType::ID},
    {"while", TokenType::ID},
    {"for", TokenType::ID},
    {"def", TokenType::ID},
    {"return", TokenType::ID},
    {"class", TokenType::ID},
    {"try", TokenType::ID},
    {"except", TokenType::ID},
    {"finally", TokenType::ID},
    {"with", TokenType::ID},
    {"as", TokenType::ID},
    {"import", TokenType::ID},
    {"from", TokenType::ID},
    {"global", TokenType::ID},
    {"nonlocal", TokenType::ID},
    {"assert", TokenType::ID},
    {"break", TokenType::ID},
    {"continue", TokenType::ID},
    {"pass", TokenType::ID},
    {"raise", TokenType::ID},
    {"yield", TokenType::ID},
    {"lambda", TokenType::ID},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"is", TokenType::IS},
    {"in", TokenType::IN},
    {"not in", TokenType::NOTIN},
    {"is not", TokenType::ISNOT}
};
