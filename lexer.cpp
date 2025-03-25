#include "lexer.hpp"
#include <stdexcept>
#include <bits/stdc++.h>
//recom:
// match eof/end (index+size < input.size) DONE
// order:
// keywords
// literal
// operator
const std::unordered_set<std::string> two_ops = {
    "==", "!=", "<=", ">=", "//", "**", "+=", "-=", "*=", "/=",
    "%=", "&=", "|=", "^=", ">>", "<<"
};

const std::unordered_set<std::string> three_ops = {
    "**=", "//=", ">>=", "<<="
};

// not in, is not пробелы
// if - tokentype::if
const std::unordered_map<std::string, TokenType> Lexer::triggers = {
    {"in", TokenType::IN},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"is", TokenType::IS},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"elif", TokenType::ELIF},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"def", TokenType::DEF},
    {"return", TokenType::RETURN},
    {"assert", TokenType::ASSERT},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"pass", TokenType::PASS},
    {"True", TokenType::BOOL}, 
    {"False", TokenType::BOOL},
    {"None", TokenType::NONE},
    {"exit", TokenType::EXIT},
    {"print", TokenType::PRINT},
    {"input", TokenType::INPUT}
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

bool Lexer::match(bool eof, std::size_t size = 0) {
    if (eof) {
        return index + size >= input.size();
    } else {
        return index + size < input.size();
    }
}

Token Lexer::extract() {
    while (index < input.size()) {

        // def hello():
        //    x = 10
        // exit()
        
        if (input[index] == '\t') {
            
            Token indentTok = extract_indentation();
            
            if (indentTok.type != TokenType::NEWLINE) {
                return indentTok;
            }
           
        }


        // Сначала обрабатываем перевод строки
        if (input[index] == '\n') {
             // newline?, 
             return extract_newline();
        }
          
        // while (input[index] == ' ')
        if (input[index] == ' ') {
            ++index;
            ++column;
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

        throw std::runtime_error(std::string("Unexpected symbol ") + input[index]);
    }
    
    return {TokenType::END, ""};
}


Token Lexer::extract_newline() {
    ++index;
    column = 1;
    ++line;
    at_line_start = true;

    return {TokenType::NEWLINE, "\\n", line - 1, column};
}

Token Lexer::extract_indentation() {
    

    int current_spaces = 0;
    while (match(false) && (input[index] == ' ' || input[index] == '\t')) {

        if (input[index] == '\t')
            current_spaces += 4;
        else
            current_spaces += 1;
        ++index;
        ++column;
    }

    if (current_spaces % 4 == 0) {
        if (current_spaces > indent_stack.back()) {
            indent_stack.push_back(current_spaces);
            return Token{TokenType::INDENT, "", line, column};
        }
        
        else if (current_spaces < indent_stack.back()) {
            while (!indent_stack.empty() && indent_stack.back() > current_spaces) {
                indent_stack.pop_back();
                pending_indent_tokens.push_back(Token{TokenType::DEDENT, "", line, column});
            }
            
            // parser problem
            //if (indent_stack.empty() || indent_stack.back() != current_spaces) {
                //throw std::runtime_error("Indentation error at line " + std::to_string(line));
            //}
            
            Token tok = pending_indent_tokens.front();
            pending_indent_tokens.erase(pending_indent_tokens.begin());
            return tok;
        }   
        return Token{TokenType::NEWLINE, "\\n", line, column};
    }
    throw std::runtime_error("Indentation error at line " + std::to_string(line));
}

Token Lexer::extract_identifier() {
    std::size_t size = 0;
    int start_col = column;
    
    while (match(false, size) && (std::isalnum(input[index + size]) || input[index + size] == '_')) {
        size++;
        column++;
    }
    
    std::string name(input, index, size);
    index += size;
    
    // if keyword -> tokentype
    auto it = triggers.find(name);

    if (it != triggers.end()) {
        return {it->second, name, line, start_col};
    }

    return {TokenType::ID, name, line, start_col};
}


Token Lexer::extract_number() {
    std::size_t size = 0;
    int start_col = column;

    while (match(false, size) && std::isdigit(input[index + size])) {
        ++size;
        ++column;
    }

    bool is_float = false;

    // dot float 4.3231
    if (match(false, size) && input[index + size] == '.') {
        is_float = true;
        ++size;
        ++column;   

        if (match(false, size) && std::isdigit(input[index + size])) {
            while (std::isdigit(input[index + size])) {
                ++size;
                ++column;
            }
        } else {
            // 123. -> 123.0

            std::string value(input, index, size);
            index += size;

            return {TokenType::FLOATNUM, value + "0", line, column};
        }

        std::string value(input, index, size);
        index += size;
        return {TokenType::FLOATNUM, value, line, column};
    }

    // exp / 4e-3 / tokentype:float
    
    if (match(false, size) && (input[index+size] == 'e' || input[index + size] == 'E')) {
        is_float = true;
        ++size;
        ++column;

        if (match(false, size) && (input[index + size] == '+' || input[index + size] == '-')) {
            ++size;
            ++column;
        }

        if (match(false, size) && std::isdigit(input[index + size])) {
            while (match(false, size) && std::isdigit(input[index + size])) {
                ++size;
                ++column;
            }
        } else {
            throw std::runtime_error("Invalid at line " + std::to_string(line));
        }

        std::string value(input, index, size);
        index += size;
        return {TokenType::FLOATNUM, value, line, column};
    } 
    

    std::string value(input, index, size);
    index += size;

    return {TokenType::INTNUM, value, line, start_col};
}


Token Lexer::extract_string() {
    char quota = input[index];
    bool is_triple = false;
    std::size_t size = 1;
    int start_col = column;

    if (match(false, 2) && input[index + 1] == quota && input[index + 2] == quota) {
        is_triple = true;
        size += 2;
        index = index + 3;
    } else {
        ++index;
    }

    
    ++column;

    std::string value;
    bool escape = false;

    while (match(false)) {
        char c = input[index];

        if (match(true)) {
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

    if (match(true, 1)) {
        std::string op_two = op + input[index + 1];

        // if (op_two.contains(two_ops))
        if (two_ops.find(op_two) != two_ops.end()) {
            op = op_two;
            size = 2;
        }
    }

    if (match(false, 2)) {
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
