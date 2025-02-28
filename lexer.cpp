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


Token Lexer::extract_number() {
    std::size_t size = 0;

    while (index + size < input.size() && std::isdigit(input[index + size])) {
        ++size;
        ++column;
    }

    bool is_float = false;

    // dot float
    if (index + size < input.size() && input[index + size] == '.') {
        is_float = true;
        ++size;
        ++column;   

        if (index + size < input.size() && std::isdigit(input[index + size])) {
            while (std::isdigit(input[index + size])) {
                ++size;
                ++column;
            }
        } else {
            // 123. -> 123.0

            std::string value(input, index, size);
            index += size;

            return {TokenType::NUM, value + "0", line, column};
        }
    }

    // exp
    if (index + size < input.size() && (input[index+size] == 'e' || input[index + size] == 'E')) {
        is_float = true;
        ++size;
        ++column;

        if (index + size < input.size() && (input[index + size] == '+' || input[index + size] == '-')) {
            ++size;
            ++column;
        }

        if (index + size < input.size() && std::isdigit(input[index + size])) {
            while (index + size < input.size() && std::isdigit(input[index + size])) {
                ++size;
                ++column;
            }
        } else {
            throw std::runtime_error("Invalid at line " + std::to_string(line));
        }
    } 
    

    std::string value(input, index, size);
    index += size;

    return {TokenType::NUM, value, line, column};
}


Token Lexer::extract_string() {
    char quota = input[index];
    bool is_triple = false;
    std::size_t size = 1;
    int start_col = column;

    if (index + 2 < input.size() && input[index + 1] == quota && input[index + 2] == quota) {
        is_triple = true;
        size += 2;
    }

    ++index;
    ++column;

    std::string value;
    bool escape = false;

    while (index < input.size()) {
        char c = input[index];

        if (!escape && c == quota) {
            if (is_triple) {
                index += 3;
                column += 3;
                break;
            } else {
                ++index;
                ++column;
                break;
            }
        } 

        if (escape) {
            switch (c) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '\\': value += '\\'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                default: value += '\\' + c; break;
            }
            escape = false;
        } else if (c = '\\') {
            escape = true;
        } else {
            value += c;
        }
        
        ++index;
        ++column;

        if (c == '\n') {
            ++line;
            column = 1;
        }
    }

    return {TokenType::STRING, value, line, start_col};
}







const std::unordered_map<std::string, TokenType> Lexer::triggers = {
    {"if", TokenType::ID},
    {"else", TokenType::ID},
    {"elif", TokenType::ID},
    {"while", TokenType::ID},
    {"for", TokenType::ID},
    {"def", TokenType::ID},
    {"return", TokenType::ID},
    {"assert", TokenType::ID},
    {"break", TokenType::ID},
    {"continue", TokenType::ID},
    {"pass", TokenType::ID},
    {"True", TokenType::ID},
    {"False", TokenType::ID},
    {"None", TokenType::ID},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"is", TokenType::IS},
    {"in", TokenType::IN},
    {"not in", TokenType::NOTIN},
    {"is not", TokenType::ISNOT},
    {"exit", TokenType::ID},
    {"print", TokenType::ID},
    {"input", TokenType::ID}
};
