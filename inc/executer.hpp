#pragma once

#include "ast.hpp"
#include "scope.hpp"
#include "error_reporter.hpp"
#include "object.hpp"
#include "type_registry.hpp"

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>


class Executor : public ASTVisitor {
public:
    Executor();

    
    void execute(TransUnit &unit);

    // вычислить одно выражение 
    std::shared_ptr<Object> evaluate(Expression &expr);

   

    void visit(TransUnit  &node) override;
    void visit(FuncDecl   &node) override;
    void visit(ClassDecl  &node) override;  
    void visit(BlockStat  &node) override;
    void visit(ExprStat   &node) override;
    void visit(AssignStat &node) override;
    void visit(IdExpr     &node) override;
    void visit(LiteralExpr&node) override;
    void visit(BinaryExpr &node) override;
    void visit(UnaryExpr  &node) override;
    void visit(CallExpr   &node) override;
    void visit(IndexExpr  &node) override;
    void visit(AttributeExpr &node) override;
    void visit(ListExpr   &node) override;
    void visit(DictExpr   &node) override;
    void visit(SetExpr    &node) override;
    void visit(TernaryExpr&node) override;

    void visit(CondStat    &node) override;
    void visit(WhileStat   &node) override;
    void visit(ForStat     &node) override;
    void visit(ReturnStat  &node) override;
    void visit(BreakStat   &node) override;
    void visit(ContinueStat&node) override;
    void visit(PassStat    &node) override;
    void visit(AssertStat  &node) override;
    void visit(ExitStat    &node) override;
    void visit(PrintStat   &node) override;

private:
    Scope scopes;                  // наша таблица символов / областей видимости
    ErrorReporter reporter;        // для накопления и печати ошибок

    std::shared_ptr<Object> lastValue;  // результат последнего вычисленного выражения

    // Пара стека-методов, чтобы хранить значение
    void pushValue(std::shared_ptr<Object> val)   { lastValue = std::move(val); }
    std::shared_ptr<Object> getValue() const      { return lastValue; }

    /**
     * Исключение, используемое для прерывания обхода при встрече `return`.
     * Внутри visit(ReturnStat) мы его бросаем,  
     * а в visit(CallExpr) ловим, чтобы вернуть управление туда.
     */
    struct ReturnSignal {
        std::shared_ptr<Object> value;
    };
};
