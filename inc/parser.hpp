#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <vector>
#include <memory>
#include <string>

class Parser {
public:
    using node = std::unique_ptr<ASTNode>;
    using funcDecl = std::unique_ptr<FuncDecl>;
    using blockStat = std::unique_ptr<BlockStat>;
    using statement = std::unique_ptr<Statement>;
    using condStat = std::unique_ptr<CondStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using forStat = std::unique_ptr<ForStat>;
    using returnStat = std::unique_ptr<ReturnStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;
    using whileStat = std::unique_ptr<WhileStat>;


    Parser(const std::vector<Token>& tokens);
private:
    const std::vector<Token>& tokens;

    std::size_t cur;

    Token peek() const;
    Token advance();
    bool is_end() const;
    bool match(const std::vector<TokenType>& types);
    Token extract(TokenType type);

    
}