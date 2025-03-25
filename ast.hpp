#pragma once

#include <string>
#include <vector>
#include <memory>

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

class Expression : public ASTNode {
public:
    virtual ~Expression() = default;
};

class Statement : public ASTNode {
public:
    virtual ~Statement() = default;
};

// <translation_unit> = (<func_decl> | <statement>)*
class TransUnit: public ASTNode {  
public:
    std::vector<std::unique_ptr<ASTNode>> units;

    TransUnit() = default;

    explicit TransUnit(std::vector<std::unique_ptr<ASTNode>> units) : units(std::move(units)) {}
};

// <func_decl> = 'def' <id> '(' <param_decl>? (',' <param_decl>)* ')' ':' <block_st>
class FuncDecl : public Statement { 
public:
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<Statement> body;

    FuncDecl(const std::string &name, const std::vector<std::string> &params, std::unique_ptr<Statement> body)
        : name(name), params(params), body(std::move(body)) {}
};

// Instructions(Statements)

// <block_st> = INDENT <statement>* DEDENT
class BlockStat : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;

    BlockStat() = default;
    
    explicit BlockStat(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}
};

// <expr_st> = <expr> NEWLINE
class ExprStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    explicit ExprStat(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}
};

// <conditional_st> = 'if' <expr> ':' <block_st> ('elif' <expr> ':' <block_st>)* ('else' ':' <block_st>)?
class CondStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStat> ifblock;
    
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<BlockStat>>> elifblocks;
    std::unique_ptr<BlockStat> elseblock;

    CondStat(std::unique_ptr<Expression> condition, std::unique_ptr<BlockStat> ifblock)
        : condition(std::move(condition)), ifblock(std::move(ifblock)) {}
};

// <while_st> = 'while' <expr> ':' <block_st>
class WhileStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStat> body;

    WhileStat(std::unique_ptr<Expression> condition, std::unique_ptr<BlockStat> body)
        : condition(std::move(condition)), body(std::move(body)) {}
};

// <for_st> = 'for' <id> 'in' <expr> ':' <block_st>
class ForStat : public Statement {
public:
    std::string iterator;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<BlockStat> body;

    ForStat(const std::string &iterator, std::unique_ptr<Expression> iterable, std::unique_ptr<BlockStat> body)
        : iterator(iterator), iterable(std::move(iterable)), body(std::move(body)) {}
};

// <return_st> = 'return' <expr>? NEWLINE
class ReturnStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    ReturnStat() = default;
    explicit ReturnStat(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}
};

// <break_st> = 'break' NEWLINE
class BreakStat : public Statement {
public:
    BreakStat() = default;
};

// <continue_st> = 'continue' NEWLINE
class ContinueStat : public Statement {
public:
    ContinueStat() = default;
};

// <pass_st> = 'pass' NEWLINE
class PassStat : public Statement {
public:
    PassStat() = default;
};

// <assert_st> = 'assert' <expr> (',' <expr>)? NEWLINE
class AssertStat : public Statement {
public:
    std::unique_ptr<Expression> condition;

    AssertStat() = default;

    explicit AssertStat(std::unique_ptr<Expression> condition) : condition(std::move(condition)) {}
};

// <exit_st> = 'exit' '(' <expr>? ')' NEWLINE   
class ExitStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    ExitStat() = default;
    explicit ExitStat(std::unique_ptr<Expression> expr)
        : expr(std::move(expr)) {}
};


// EXPRESSIONS

class OrExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> rights;

    explicit OrExpr(std::unique_ptr<Expression> left) : left(std::move(left)) {}
};

class AndExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> rights;
    explicit AndExpr(std::unique_ptr<Expression> left) : left(std::move(left)) {}
};

class NotExpr : public Expression {
public:
    std::unique_ptr<Expression> comparison;
    explicit NotExpr(std::unique_ptr<Expression> comparison) : comparison(std::move(comparison)) {}
};

class ComparisonExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> rights;
    explicit ComparisonExpr(std::unique_ptr<Expression> left) : left(std::move(left)) {}
};


class ArithExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    // Пара: '+' или '-' и правый операнд
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> rights;
    explicit ArithExpr(std::unique_ptr<Expression> left) : left(std::move(left)) {}
};


// Тернарное выражение: <expr> 'if' <expr> 'else' <expr>
class TernaryExpr : public Expression {
public:
    std::unique_ptr<Expression> trueExpr;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> falseExpr;

    TernaryExpr(std::unique_ptr<Expression> trueExpr,
                std::unique_ptr<Expression> condition,
                std::unique_ptr<Expression> falseExpr)
        : trueExpr(std::move(trueExpr)), condition(std::move(condition)), falseExpr(std::move(falseExpr)) {}
};



// Идентификатор: <id>
class IdExpr : public Expression {
public:
    std::string name;

    explicit IdExpr(const std::string &name)
        : name(name) {}
};



// <term> = <factor> (('*' | '/' | '//' | '%') <factor>)*
class TermExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> rights;

    explicit TermExpr(std::unique_ptr<Expression> left)
        : left(std::move(left)) {}
};


// <factor> = (('+' | '-') <factor>)? <power>
class FactorExpr : public Expression {
public:
    
    std::string unaryOp;
    std::unique_ptr<Expression> operand;

    FactorExpr(const std::string &unaryOp, std::unique_ptr<Expression> operand)
        : unaryOp(unaryOp), operand(std::move(operand)) {}
};


// <power> = <primary> ('**' <factor>)?
class PowerExpr : public Expression {
public:
    std::unique_ptr<Expression> base;
    
    std::unique_ptr<Expression> exponent;

    explicit PowerExpr(std::unique_ptr<Expression> base)
        : base(std::move(base)) {}

    PowerExpr(std::unique_ptr<Expression> base, std::unique_ptr<Expression> exponent)
        : base(std::move(base)), exponent(std::move(exponent)) {}
};


// <primary> = <literal_expr> | <id_expr> | <call_expr> | <index_expr> | '(' <expr> ')' | <ternary_expr>
class PrimaryExpr : public Expression {
public:
    
    enum class PrimaryType { LITERAL, ID, CALL, INDEX, PAREN, TERNARY };
    PrimaryType type;

    
    std::unique_ptr<Expression> literalExpr;
    
    std::unique_ptr<Expression> idExpr;
    
    std::unique_ptr<Expression> callExpr;
    
    std::unique_ptr<Expression> indexExpr;
    
    std::unique_ptr<Expression> parenExpr;
    
    std::unique_ptr<Expression> ternaryExpr;

    explicit PrimaryExpr(std::unique_ptr<Expression> literalExpr)
        : type(PrimaryType::LITERAL), literalExpr(std::move(literalExpr)) {}

    
    explicit PrimaryExpr(const std::string &id)
        : type(PrimaryType::ID), idExpr(std::make_unique<IdExpr>(id)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> callExpr, bool isCall)
        : type(PrimaryType::CALL), callExpr(std::move(callExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> indexExpr, int dummy)
        : type(PrimaryType::INDEX), indexExpr(std::move(indexExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> parenExpr)
        : type(PrimaryType::PAREN), parenExpr(std::move(parenExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> ternaryExpr, double dummy)
        : type(PrimaryType::TERNARY), ternaryExpr(std::move(ternaryExpr)) {}
};



class CallExpr : public Expression {
public:
    std::unique_ptr<Expression> caller; // Обычно это идентификатор
    std::vector<std::unique_ptr<Expression>> arguments;

    CallExpr(std::unique_ptr<Expression> caller, std::vector<std::unique_ptr<Expression>> arguments)
        : caller(std::move(caller)), arguments(std::move(arguments)) {}
};

// <index_expr> = <id_expr> '[' <expr> ']'
class IndexExpr : public Expression {
public:
    std::unique_ptr<Expression> base;  // Обычно идентификатор
    std::unique_ptr<Expression> index;

    IndexExpr(std::unique_ptr<Expression> base, std::unique_ptr<Expression> index)
        : base(std::move(base)), index(std::move(index)) {}
};


class LiteralExpr : public Expression {
public:
    enum class LiteralType { INT, FLOAT, STRING, BOOL, NONE };
    LiteralType literalType;
    std::string value;

    LiteralExpr(LiteralType literalType, const std::string &value)
        : literalType(literalType), value(value) {}
};





