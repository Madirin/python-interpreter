#include "lexer.hpp"
#include <stdexcept>
#include <bits/stdc++.h>

const std::unordered_set<std::string> two_ops = {
    "==", "!=", "<=", ">=", "//", "**", "+=", "-=", "*=", "/=",
    "%=", "&=", "|=", "^=", ">>", "<<"
};

const std::unordered_set<std::string> three_ops = {
    "**=", "//=", ">>=", "<<="
};


const std::unordered_map<std::string, TokenType> Lexer::triggers = {
    {"not in", TokenType::NOTIN},
    {"is not", TokenType::ISNOT},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"is", TokenType::IS},
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
    {"in", TokenType::IN},
    {"exit", TokenType::ID},
    {"print", TokenType::ID},
    {"input", TokenType::ID}
};


const std::unordered_map<std::string, TokenType> operator_map = {
    // Арифметические операторы
    {"+", TokenType::PLUS},   {"-", TokenType::MINUS},   {"*", TokenType::STAR},   {"/", TokenType::SLASH},
    {"//", TokenType::DOUBLESLASH}, {"%", TokenType::MOD}, {"**", TokenType::POW}, {"=", TokenType::ASSIGN},

    // Сравнение
    {"==", TokenType::EQUAL}, {"!=", TokenType::NOTEQUAL}, {"<", TokenType::LESS}, {">", TokenType::GREATER},
    {"<=", TokenType::LESSEQUAL}, {">=", TokenType::GREATEREQUAL},

    // Составные операторы
    {"+=", TokenType::PLUSEQUAL}, {"-=", TokenType::MINUSEQUAL}, {"*=", TokenType::STAREQUAL},
    {"/=", TokenType::SLASHEQUAL}, {"//=", TokenType::DOUBLESLASHEQUAL}, {"%=", TokenType::MODEQUAL},
    {"**=", TokenType::POWEQUAL},

    // Битовые операторы
    {"&=", TokenType::AND}, {"|=", TokenType::OR}, {"^=", TokenType::NOT},
    {">>", TokenType::IS}, {"<<", TokenType::ISNOT}, {">>=", TokenType::IS}, {"<<=", TokenType::ISNOT},

    // Скобки (приоритет операций)
    {"(", TokenType::LPAREN}, {")", TokenType::RPAREN},
    {"[", TokenType::LBRACKET}, {"]", TokenType::RBRACKET},
    {"{", TokenType::LBRACE}, {"}", TokenType::RBRACE},

    // Разделители
    {",", TokenType::COMMA}, {":", TokenType::COLON},
    {".", TokenType::DOT}
};

Lexer::Lexer(const std::string& input) : input(input) {
    indent_stack.push_back(0);  
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (index < input.size()) {
        tokens.push_back(extract());
    }

    return tokens;
}

Token Lexer::extract() {
    while (index < input.size()) {
        
        if (!pending_indent_tokens.empty()) {
            Token tok = pending_indent_tokens.front();
            pending_indent_tokens.erase(pending_indent_tokens.begin());
            return tok;
        }

        // Сначала обрабатываем перевод строки
        if (input[index] == '\n') {
            return extract_newline();
        }
        // Если начало строки (column == 1) и есть пробелы/табуляции, обрабатываем отступ
        if (column == 1 && (input[index] == ' ' || input[index] == '\t')) {
            return extract_indentation();
        } else if (input[index] == ' ') {
            ++index;
            ++column;
        }
        // Дальше обрабатываем идентификаторы, числа, строки, операторы и т.д.
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

        throw std::runtime_error(std::string("Unexpected symbol ") + input[index]);
    }
    
    return {TokenType::END, ""};
}


Token Lexer::extract_newline() {
    ++index;
    column = 1;
    ++line;

    return {TokenType::NEWLINE, "\\n", line - 1, column};
}

Token Lexer::extract_indentation() {
    if (!pending_indent_tokens.empty()) {
        Token tok = pending_indent_tokens.front();
        pending_indent_tokens.erase(pending_indent_tokens.begin());
        return tok;
    }

    int current_spaces = 0;
    while (index < input.size() && (input[index] == ' ' || input[index] == '\t')) {

        if (input[index] == '\t')
            current_spaces += 4;
        else
            current_spaces += 1;
        ++index;
        ++column;
    }

    if (current_spaces > indent_stack.back()) {
        indent_stack.push_back(current_spaces);
        return Token{TokenType::INDENT, "", line, column};
    }
    // Если уровень уменьшился, нужно выдать один или несколько DEDENT токенов
    else if (current_spaces < indent_stack.back()) {
        while (!indent_stack.empty() && indent_stack.back() > current_spaces) {
            indent_stack.pop_back();
            pending_indent_tokens.push_back(Token{TokenType::DEDENT, "", line, column});
        }
        // Если после "выемки" из стека текущий уровень не совпадает с найденным, это ошибка отступов
        if (indent_stack.empty() || indent_stack.back() != current_spaces) {
            throw std::runtime_error("Indentation error at line " + std::to_string(line));
        }
        // Отдаём первый DEDENT из накопленного списка
        Token tok = pending_indent_tokens.front();
        pending_indent_tokens.erase(pending_indent_tokens.begin());
        return tok;
    }
    // Если отступ не изменился — просто вернуть специальный токен (например, NEWLINE) или продолжить обработку
    return Token{TokenType::NEWLINE, "\\n", line, column};
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

        if (index >= input.size()) {
            throw std::runtime_error("Unterminated string at line " + std::to_string(line));
        }
        
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
        } else if (c == '\\') {
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

Token Lexer::extract_operator() {
    int start_col = column;
    std::size_t size = 1;
    std::string op;

    op += input[index];

    if (index + 1 < input.size()) {
        std::string op_two = op + input[index + 1];

        if (two_ops.find(op_two) != two_ops.end()) {
            op = op_two;
            size = 2;
        }
    }

    if (index + 2 < input.size()) {
        std::string op_three = op + input[index + 2];

        if (three_ops.find(op_three) != three_ops.end()) {
            op = op_three;
            size = 3;
        }
    }

    index += size;
    column += size;

    auto it = operator_map.find(op);
    if (it != operator_map.end()) {
        return Token{it->second, op, line, start_col};

    }

    throw std::runtime_error("Unknown operator: " + op + " at line " + std::to_string(line));
}
