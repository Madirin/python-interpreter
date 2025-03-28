#include "parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), cur(0) {}

Token Parser::peek() const {
    if (cur < tokens.size()) {
        return tokens[cur];
    }

    throw std::runtime_error("peek - don't have any tokens left");
}

Token Parser::advance() {
    if (!is_end()) {
        return tokens[cur++];
    }
    throw std::runtime_error("advance - nowhere to advance");
}

bool Parser::is_end() const {
    return cur >= tokens.size(); 
}

bool Parser::match(const std::vector<TokenType>& types) {
    if (is_end()) {
        return false;
    }

    token current = peek();

    for (TokenType type : types) {
        if (current.type == type) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::extract(TokenType type) {
    if(is_end()) {
        throw std::runtime_error("extract - don't have tokens");
    }
    if (peek().type != type) {
        throw std::runtime_error("extract - wrong type");
    }
}