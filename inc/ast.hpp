#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

// for variant constructors for primexpr
struct CallTag {};
struct IndexTag {};
struct ParenTag {};
struct TernaryTag {};

// file astvisitor
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual void visit(class TransUnit &node) = 0;
    virtual void visit(class FuncDecl &node) = 0;
    virtual void visit(class BlockStat &node) = 0;
    virtual void visit(class ExprStat &node) = 0;
    virtual void visit(class CondStat &node) = 0;
    virtual void visit(class WhileStat &node) = 0;
    virtual void visit(class ForStat &node) = 0;
    virtual void visit(class ReturnStat &node) = 0;
    virtual void visit(class BreakStat &node) = 0;
    virtual void visit(class ContinueStat &node) = 0;
    virtual void visit(class PassStat &node) = 0;
    virtual void visit(class AssertStat &node) = 0;
    virtual void visit(class ExitStat &node) = 0;
    virtual void visit(class PrintStat &node) = 0;
    virtual void visit(class AssignStat &node) = 0;
    

    virtual void visit(class UnaryExpr &node) = 0;
    virtual void visit(class BinaryExpr &node) = 0;
    virtual void visit(class PrimaryExpr &node) = 0;
    virtual void visit(class TernaryExpr &node) = 0;
    virtual void visit(class IdExpr &node) = 0;
    virtual void visit(class LiteralExpr &node) = 0;
    virtual void visit(class CallExpr &node) = 0;
    virtual void visit(class IndexExpr &node) = 0;
    virtual void visit(class AttributeExpr &node) = 0;

    virtual void visit(class ListExpr &node) = 0;
    virtual void visit(class SetExpr  &node) = 0;
    virtual void visit(class DictExpr &node) = 0;

};

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor &visitor) = 0;
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

    TransUnit(std::vector<std::unique_ptr<ASTNode>> units) : units(std::move(units)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <func_decl> = 'def' <id> '(' <param_decl>? (',' <param_decl>)* ')' ':' <block_st>
class FuncDecl : public Statement { 
public:
    std::string name;
    std::vector<std::string> posParams; 
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> defaultParams; // default params
    std::unique_ptr<Statement> body;
    int line;

    FuncDecl(
        const std::string &name,
        std::vector<std::string> posParams,
        std::vector<std::pair<std::string, std::unique_ptr<Expression>>> defaultParams,
        std::unique_ptr<Statement> body,
        int line
    )
        : name(name)
        , posParams(posParams)
        , defaultParams(std::move(defaultParams))
        , body(std::move(body))
        , line(line)
    {}
    
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// Instructions(Statements)

// <block_st> = INDENT <statement>* DEDENT
class BlockStat : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;

    BlockStat() = default;
    
    BlockStat(std::vector<std::unique_ptr<Statement>> statements): statements(std::move(statements)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <expr_st> = <expr> NEWLINE
class ExprStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    ExprStat(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
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
    
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <while_st> = 'while' <expr> ':' <block_st> 
// LOGGING: NEED TO ADD ELSE
class WhileStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStat> body;

    WhileStat(std::unique_ptr<Expression> condition, std::unique_ptr<BlockStat> body)
        : condition(std::move(condition)), body(std::move(body)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <for_st> = 'for' <id> 'in' <expr> ':' <block_st>
// i, j, k in enumerator LOGGING
class ForStat : public Statement {
public:
    std::vector<std::string> iterators;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<BlockStat> body;

    ForStat(std::vector<std::string> iterators, std::unique_ptr<Expression> iterable, std::unique_ptr<BlockStat> body)
        : iterators(iterators), iterable(std::move(iterable)), body(std::move(body)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <return_st> = 'return' <expr>? NEWLINE
class ReturnStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    ReturnStat() = default;
    ReturnStat(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <break_st> = 'break' NEWLINE
class BreakStat : public Statement {
public:
    BreakStat() = default;
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <continue_st> = 'continue' NEWLINE
class ContinueStat : public Statement {
public:
    ContinueStat() = default;
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <pass_st> = 'pass' NEWLINE
class PassStat : public Statement {
public:
    PassStat() = default;
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <assert_st> = 'assert' <expr> (',' <expr>)? NEWLINE

class AssertStat : public Statement {
public:
    std::unique_ptr<Expression> condition;

    AssertStat() = default;

    AssertStat(std::unique_ptr<Expression> condition) : condition(std::move(condition)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <exit_st> = 'exit' '(' <expr>? ')' NEWLINE   
class ExitStat : public Statement {
public:
    std::unique_ptr<Expression> expr;

    ExitStat() = default;
    ExitStat(std::unique_ptr<Expression> expr)
        : expr(std::move(expr)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

class PrintStat : public Statement {
public:
    std::unique_ptr<Expression> expr;
    PrintStat() = default;
    PrintStat(std::unique_ptr<Expression> expr)
        : expr(std::move(expr)) {}
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};


// EXPRESSIONS

class UnaryExpr : public Expression {
public:
    std::string op;
    std::unique_ptr<Expression> operand;
    int line;

    UnaryExpr(std::string op, std::unique_ptr<Expression> operand, int line)
        : op(op), operand(std::move(operand)), line(line) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); };
};

class BinaryExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::string op;
    std::unique_ptr<Expression> right;
    int line;

    BinaryExpr(std::unique_ptr<Expression> left,
               std::string op,
               std::unique_ptr<Expression> right, 
               int line)
        : left(std::move(left)), op(op), right(std::move(right)), line(line) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); };
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

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};



// Идентификатор: <id>
class IdExpr : public Expression {
public:
    std::string name;
    int line;

    IdExpr(const std::string &name, int line)
        : name(name), line(line) {}

    explicit IdExpr(const std::string &name)
      : name(name), line(0) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

class AssignStat : public Statement {
public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    int line;
            
    AssignStat(std::unique_ptr<Expression> left, std::unique_ptr<Expression> right, int line): left(std::move(left)), right(std::move(right)), line(line) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
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

    PrimaryExpr(std::unique_ptr<Expression> literalExpr)
        : type(PrimaryType::LITERAL), literalExpr(std::move(literalExpr)) {}

    
    PrimaryExpr(const std::string &id)
        : type(PrimaryType::ID), idExpr(std::make_unique<IdExpr>(id)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> callExpr, CallTag)
        : type(PrimaryType::CALL), callExpr(std::move(callExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> indexExpr, IndexTag)
        : type(PrimaryType::INDEX), indexExpr(std::move(indexExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression>, ParenTag)
        : type(PrimaryType::PAREN), parenExpr(std::move(parenExpr)) {}

    
    PrimaryExpr(std::unique_ptr<Expression> ternaryExpr, TernaryTag)
        : type(PrimaryType::TERNARY), ternaryExpr(std::move(ternaryExpr)) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};



class CallExpr : public Expression {
public:
    std::unique_ptr<Expression> caller;
    std::vector<std::unique_ptr<Expression>> arguments;
    int line;

    CallExpr(std::unique_ptr<Expression> caller, std::vector<std::unique_ptr<Expression>> arguments, int line)
        : caller(std::move(caller)), arguments(std::move(arguments)), line(line) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// <index_expr> = <id_expr> '[' <expr> ']'
class IndexExpr : public Expression {
public:
    std::unique_ptr<Expression> base;  
    std::unique_ptr<Expression> index;
    int line;

    IndexExpr(std::unique_ptr<Expression> base, std::unique_ptr<Expression> index, int line)
        : base(std::move(base)), index(std::move(index)), line(line) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};


class LiteralExpr : public Expression {
public:
    // enum class LiteralType { INT, FLOAT, STRING, BOOL, NONE };
    using Value = std::variant<int, double, std::string, bool, std::monostate>;
    Value value;
    int line;

    LiteralExpr(int v, int line): value(v), line(line) {}
    LiteralExpr(double v, int line): value(v), line(line) {}
    LiteralExpr(std::string v, int line): value(v), line(line) {}
    LiteralExpr(bool v, int line): value(v), line(line) {}
    LiteralExpr(int line): value(std::monostate{}), line(line) {}

        
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

class AttributeExpr : public Expression {
public:
    std::unique_ptr<Expression> obj;
    std::string name;
    

    AttributeExpr(std::unique_ptr<Expression> obj, std::string name) : obj(std::move(obj)), name(name) {}

    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// [expr0, expr1, expr2]
class ListExpr : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elems;
    ListExpr(std::vector<std::unique_ptr<Expression>> elems) : elems(std::move(elems)) {}
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

// {expr, expr, expr}
class SetExpr : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elems;
    SetExpr(std::vector<std::unique_ptr<Expression>> elems) : elems(std::move(elems)) {}
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};

class DictExpr : public Expression {
public:
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
    DictExpr(std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items) : items(std::move(items)) {}
    virtual void accept(ASTVisitor &visitor) override { visitor.visit(*this); }
};






