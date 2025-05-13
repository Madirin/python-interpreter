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
    using breakStat = std::unique_ptr<BreakStat>;
    using continueStat = std::unique_ptr<ContinueStat>;
    using passStat = std::unique_ptr<PassStat>;
    using assertStat = std::unique_ptr<AssertStat>;
    using exitStat = std::unique_ptr<ExitStat>;
    using exprStat = std::unique_ptr<ExprStat>;
    using printStat = std::unique_ptr<PrintStat>;
    using expression = std::unique_ptr<Expression>;
    using idExpr = std::unique_ptr<IdExpr>;

    std::unique_ptr<TransUnit> parse();
    Parser(const std::vector<Token>& tokens);
private:
    const std::vector<Token>& tokens;

    std::size_t cur;

    Token peek() const;
    Token peek_next() const;
    Token advance();
    bool is_end() const;

    // template <typename... Types>
    // bool match(Types... types)
    template<typename... Types>
    bool match(Types... types);

    Token extract(TokenType type);

    funcDecl parse_func_decl();
    void parse_param_decl(std::vector<std::string> &pos_params, std::vector<std::pair<std::string, std::unique_ptr<Expression>>> &def_params);

    blockStat parse_block();
    statement parse_stat();
    exprStat parse_expr_stat();
    condStat parse_cond();
    whileStat parse_while();
    forStat parse_for();
    returnStat parse_return();
    breakStat parse_break();
    continueStat parse_continue();
    passStat parse_pass();
    assertStat parse_assert();
    exitStat parse_exit();
    printStat parse_print();
    // inputStat parse_input();
    // later on builtin funcs will be callexpr
    expression parse_expression();

    // binary, unary
    // разные функции норм
    // объединить в один binary/unary
    expression parse_or();
    expression parse_and();
    expression parse_not();

    expression parse_comparison();

    expression parse_arith();
    expression parse_term();
    expression parse_factor();
    expression parse_power();

    expression parse_primary();
    expression parse_postfix(expression given_id);
    expression parse_ternary();

    expression parse_call(expression caller);

};