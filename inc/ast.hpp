#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

// Теги для конструктора PrimaryExpr (не меняем)
struct CallTag {};
struct IndexTag {};
struct ParenTag {};
struct TernaryTag {};


// Класс-посетитель (оставляем без изменений)
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Для каждого типа узла AST есть свой метод visit(...)
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
    virtual void visit(class ClassDecl &node) = 0;
};

// Базовый узел AST
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor &visitor) = 0;
};

// Базовый класс для выражений
class Expression : public ASTNode {
public:
    virtual ~Expression() = default;
};

// Базовый класс для операторов (statements)
class Statement : public ASTNode {
public:
    virtual ~Statement() = default;
};


// -------------------------
// <translation_unit> = (<func_decl> | <class_decl> | <statement>)*
// -------------------------
class TransUnit: public ASTNode {
public:
    // Список «высокоуровневых» элементов: могут быть FuncDecl, ClassDecl или любой Statement
    std::vector<std::unique_ptr<ASTNode>> units;

    int line;  // мы добавляем, например, номер первой строки в юните (по желанию)

    TransUnit(int line = 0)
        : line(line)
    {}

    TransUnit(std::vector<std::unique_ptr<ASTNode>> units, int line)
        : units(std::move(units)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <func_decl> = 'def' <id> '(' <param_decl>? (',' <param_decl>)* ')' ':' <block_st>
// -------------------------
class FuncDecl : public Statement {
public:
    std::string name;
    std::vector<std::string> posParams;
    // defaultParams: пара {имя параметра, Expression* для значения по умолчанию}
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> defaultParams;
    std::unique_ptr<Statement> body;
    int line;  // номер строки, где стоит «def»

    FuncDecl(
        const std::string &name,
        std::vector<std::string> posParams,
        std::vector<std::pair<std::string, std::unique_ptr<Expression>>> defaultParams,
        std::unique_ptr<Statement> body,
        int line
    )
        : name(name)
        , posParams(std::move(posParams))
        , defaultParams(std::move(defaultParams))
        , body(std::move(body))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <block_st> = INDENT <statement>* DEDENT
// -------------------------
class BlockStat : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    int line;  // номер строки, где начался блок (INDENT)

    BlockStat(int line = 0)
        : line(line)
    {}

    BlockStat(std::vector<std::unique_ptr<Statement>> statements, int line)
        : statements(std::move(statements)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <expr_st> = <expr> NEWLINE
// -------------------------
class ExprStat : public Statement {
public:
    std::unique_ptr<Expression> expr;
    int line;  // номер строки, где началось выражение

    ExprStat(std::unique_ptr<Expression> expr, int line)
        : expr(std::move(expr)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <conditional_st> = 'if' <expr> ':' <block_st> ('elif' <expr> ':' <block_st>)* ('else' ':' <block_st>)?
// -------------------------
class CondStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStat>   ifblock;
    // Каждый элемент пары: {условие, блок}
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<BlockStat>>> elifblocks;
    std::unique_ptr<BlockStat> elseblock;
    int line;  // номер строки, где стоит «if»

    CondStat(std::unique_ptr<Expression> condition,
             std::unique_ptr<BlockStat> ifblock,
             int line)
        : condition(std::move(condition))
        , ifblock(std::move(ifblock))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <while_st> = 'while' <expr> ':' <block_st>
// -------------------------
class WhileStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<BlockStat> body;
    int line;  // номер строки, где стоит «while»

    WhileStat(std::unique_ptr<Expression> condition,
              std::unique_ptr<BlockStat> body,
              int line)
        : condition(std::move(condition))
        , body(std::move(body))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <for_st> = 'for' <id> (',' <id>)* 'in' <expr> ':' <block_st>
// -------------------------
class ForStat : public Statement {
public:
    std::vector<std::string> iterators;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<BlockStat> body;
    int line;  // номер строки, где стоит «for»

    ForStat(std::vector<std::string> iterators,
            std::unique_ptr<Expression> iterable,
            std::unique_ptr<BlockStat> body,
            int line)
        : iterators(std::move(iterators))
        , iterable(std::move(iterable))
        , body(std::move(body))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <return_st> = 'return' <expr>? NEWLINE
// -------------------------
class ReturnStat : public Statement {
public:
    std::unique_ptr<Expression> expr;  // при return X
    int line;  // номер строки, где стоит «return»

    ReturnStat(int line)
        : expr(nullptr), line(line)
    {}

    ReturnStat(std::unique_ptr<Expression> expr, int line)
        : expr(std::move(expr)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <break_st> = 'break' NEWLINE
// -------------------------
class BreakStat : public Statement {
public:
    int line;  // номер строки, где стоит «break»

    BreakStat(int line)
        : line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <continue_st> = 'continue' NEWLINE
// -------------------------
class ContinueStat : public Statement {
public:
    int line;  // номер строки, где стоит «continue»

    ContinueStat(int line)
        : line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <pass_st> = 'pass' NEWLINE
// -------------------------
class PassStat : public Statement {
public:
    int line;  // номер строки, где стоит «pass»

    PassStat(int line)
        : line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <assert_st> = 'assert' <expr> (',' <expr>)? NEWLINE
// -------------------------
class AssertStat : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> message; // необязательное сообщение
    int line;  // номер строки, где стоит «assert»

    AssertStat(int line)
        : condition(nullptr), message(nullptr), line(line)
    {}

    AssertStat(std::unique_ptr<Expression> condition,
               std::unique_ptr<Expression> message,
               int line)
        : condition(std::move(condition)), message(std::move(message)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <exit_st> = 'exit' '(' <expr>? ')' NEWLINE
// -------------------------
class ExitStat : public Statement {
public:
    std::unique_ptr<Expression> expr;  // exit(expr)
    int line;  // номер строки, где стоит «exit»

    ExitStat(int line)
        : expr(nullptr), line(line)
    {}

    ExitStat(std::unique_ptr<Expression> expr, int line)
        : expr(std::move(expr)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <print_st> = 'print' '(' <expr>? ')' NEWLINE
// -------------------------
class PrintStat : public Statement {
public:
    std::unique_ptr<Expression> expr; // print(expr)
    int line;  // номер строки, где стоит «print»

    PrintStat(int line)
        : expr(nullptr), line(line)
    {}

    PrintStat(std::unique_ptr<Expression> expr, int line)
        : expr(std::move(expr)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// <assign_st> = <id_expr> '=' <expr> NEWLINE
// или        = <index_expr> '=' <expr> NEWLINE
// или        = <attribute_expr> '=' <expr> NEWLINE
// -------------------------
class AssignStat : public Statement {
public:
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    int line;  // номер строки, где стоит оператор «=»

    AssignStat(std::unique_ptr<Expression> left,
               std::unique_ptr<Expression> right,
               int line)
        : left(std::move(left)), right(std::move(right)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// -------------------------
// EXPRESSION-УЗЛЫ
// -------------------------

// <unary_expr> = ('+' | '-' | 'not') <operand>
class UnaryExpr : public Expression {
public:
    std::string op;
    std::unique_ptr<Expression> operand;
    int line;  // номер строки, где стоит унарный оператор

    UnaryExpr(const std::string &op,
              std::unique_ptr<Expression> operand,
              int line)
        : op(op), operand(std::move(operand)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <binary_expr> = <left> ('+' | '-' | '*' | '/' | '==' | ... ) <right>
class BinaryExpr : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::string op;
    std::unique_ptr<Expression> right;
    int line;  // номер строки, где стоит бинарный оператор

    BinaryExpr(std::unique_ptr<Expression> left,
               const std::string &op,
               std::unique_ptr<Expression> right,
               int line)
        : left(std::move(left)), op(op), right(std::move(right)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// Тернарный оператор: <trueExpr> 'if' <condition> 'else' <falseExpr>
class TernaryExpr : public Expression {
public:
    std::unique_ptr<Expression> trueExpr;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> falseExpr;
    int line;  // номер строки, где стоит «if» в тернаре

    TernaryExpr(std::unique_ptr<Expression> trueExpr,
                std::unique_ptr<Expression> condition,
                std::unique_ptr<Expression> falseExpr,
                int line)
        : trueExpr(std::move(trueExpr))
        , condition(std::move(condition))
        , falseExpr(std::move(falseExpr))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <id_expr> = идентификатор
class IdExpr : public Expression {
public:
    std::string name;
    int line;  // номер строки, где стоит идентификатор

    IdExpr(const std::string &name, int line)
        : name(name), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <literal_expr> = целое | вещественное | строка | bool | None
class LiteralExpr : public Expression {
public:
    // Возможные варианты литерала
    using Value = std::variant<int, double, std::string, bool, std::monostate>;
    Value value;
    int line;  // номер строки, где стоит литерал

    LiteralExpr(int v, int line)
        : value(v), line(line)
    {}

    LiteralExpr(double v, int line)
        : value(v), line(line)
    {}

    LiteralExpr(const std::string &v, int line)
        : value(v), line(line)
    {}

    LiteralExpr(bool v, int line)
        : value(v), line(line)
    {}

    // None
    LiteralExpr(int line)
        : value(std::monostate{}), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <attribute_expr> = <obj> '.' <name>
class AttributeExpr : public Expression {
public:
    std::unique_ptr<Expression> obj;
    std::string name;
    int line;  // номер строки, где стоит точка

    AttributeExpr(std::unique_ptr<Expression> obj,
                  const std::string &name,
                  int line)
        : obj(std::move(obj)), name(name), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <call_expr> = <caller> '(' <arguments>* ')'
class CallExpr : public Expression {
public:
    std::unique_ptr<Expression> caller;
    std::vector<std::unique_ptr<Expression>> arguments;
    int line;  // номер строки, где стоит открывающая скобка '('

    CallExpr(std::unique_ptr<Expression> caller,
             std::vector<std::unique_ptr<Expression>> arguments,
             int line)
        : caller(std::move(caller))
        , arguments(std::move(arguments))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <index_expr> = <base> '[' <index> ']'
class IndexExpr : public Expression {
public:
    std::unique_ptr<Expression> base;
    std::unique_ptr<Expression> index;
    int line;  // номер строки, где стоит '['

    IndexExpr(std::unique_ptr<Expression> base,
              std::unique_ptr<Expression> index,
              int line)
        : base(std::move(base))
        , index(std::move(index))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <primary_expr> = <literal_expr> | <id_expr> | <call_expr> | <index_expr> | '(' <expr> ')' | <ternary_expr>
class PrimaryExpr : public Expression {
public:
    enum class PrimaryType { LITERAL, ID, CALL, INDEX, PAREN, TERNARY };
    PrimaryType type;
    int line;  // номер строки, где находится «начало» primary

    std::unique_ptr<Expression> literalExpr;
    std::unique_ptr<Expression> idExpr;
    std::unique_ptr<Expression> callExpr;
    std::unique_ptr<Expression> indexExpr;
    std::unique_ptr<Expression> parenExpr;
    std::unique_ptr<Expression> ternaryExpr;

    // Каждый конструктор задаёт свой вариант type и устанавливает поле line

    // 1) Литерал
    PrimaryExpr(std::unique_ptr<Expression> literalExpr, int line)
        : type(PrimaryType::LITERAL)
        , line(line)
        , literalExpr(std::move(literalExpr))
    {}

    // 2) Идентификатор
    PrimaryExpr(const std::string &id, int line)
        : type(PrimaryType::ID)
        , line(line)
        , idExpr(std::make_unique<IdExpr>(id, line))
    {}

    // 3) Вызов функции
    PrimaryExpr(std::unique_ptr<Expression> callExpr, CallTag, int line)
        : type(PrimaryType::CALL)
        , line(line)
        , callExpr(std::move(callExpr))
    {}

    // 4) Индексирование
    PrimaryExpr(std::unique_ptr<Expression> indexExpr, IndexTag, int line)
        : type(PrimaryType::INDEX)
        , line(line)
        , indexExpr(std::move(indexExpr))
    {}

    // 5) Скобки: '(' expr ')'
    PrimaryExpr(std::unique_ptr<Expression> parenExpr, ParenTag, int line)
        : type(PrimaryType::PAREN)
        , line(line)
        , parenExpr(std::move(parenExpr))
    {}

    // 6) Тернарный оператор
    PrimaryExpr(std::unique_ptr<Expression> ternaryExpr, TernaryTag, int line)
        : type(PrimaryType::TERNARY)
        , line(line)
        , ternaryExpr(std::move(ternaryExpr))
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <list_expr> = '[' (<expr> (',' <expr>)*)? ']'
class ListExpr : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elems;
    int line;  // номер строки, где стоит '['

    ListExpr(std::vector<std::unique_ptr<Expression>> elems, int line)
        : elems(std::move(elems)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <set_expr> = '{' <expr> (',' <expr>)* '}'
class SetExpr : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elems;
    int line;  // номер строки, где стоит '{'

    SetExpr(std::vector<std::unique_ptr<Expression>> elems, int line)
        : elems(std::move(elems)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <dict_expr> = '{' [ <expr> ':' <expr> (',' <expr> ':' <expr>)* ] '}'
class DictExpr : public Expression {
public:
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
    int line;  // номер строки, где стоит '{'

    DictExpr(std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items, int line)
        : items(std::move(items)), line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


// <field_decl> внутри класса
class FieldDecl : public Statement {
public:
    std::string name;
    std::unique_ptr<Expression> initExpr;
    int line;  // номер строки, где стоит имя поля

    FieldDecl(const std::string &name,
              std::unique_ptr<Expression> initExpr,
              int line)
        : name(name)
        , initExpr(std::move(initExpr))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        // пока не нужен, но чтобы не сломать сборку:
        // visitor.visit(*this);
    }
};


// -------------------------
// <class_decl> = 'class' ID ('(' ID (',' ID)* ')')? ':' NEWLINE <block_st>
// -------------------------
class ClassDecl : public Statement {
public:
    std::string name;
    std::vector<std::string> baseClasses;
    std::vector<std::unique_ptr<FieldDecl>> fields;
    std::vector<std::unique_ptr<FuncDecl>> methods;
    int line;  // номер строки, где стоит «class»

    ClassDecl(const std::string &name,
              std::vector<std::string> baseClasses,
              std::vector<std::unique_ptr<FieldDecl>> fields,
              std::vector<std::unique_ptr<FuncDecl>> methods,
              int line)
        : name(name)
        , baseClasses(std::move(baseClasses))
        , fields(std::move(fields))
        , methods(std::move(methods))
        , line(line)
    {}

    virtual void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
