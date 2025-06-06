#pragma once

#include "object.hpp"
#include "ast.hpp"
#include "scope.hpp"
#include "error_reporter.hpp"
#include "type_registry.hpp"
#include "executer_excepts.hpp"

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>


class Executor : public ASTVisitor {
public:

    Scope scopes;                  // наша таблица символов / областей видимости

    Executor();

    
    void execute(TransUnit &unit);

    // вычислить одно выражение 
    std::shared_ptr<Object> evaluate(Expression &expr);

    void push_value(std::shared_ptr<Object> val) {
        value_stack.push_back(std::move(val));
    }

    std::shared_ptr<Object> pop_value() {
        if (value_stack.empty()) {
            throw RuntimeError("pop_value : empty stack\n");
        }

        auto tmp = value_stack.back();
        value_stack.pop_back();
        return tmp;
    }

    std::shared_ptr<Object> peek_value() const {
        if (value_stack.empty()) {
            throw RuntimeError("peek_value: empty stack");
        }

        return value_stack.back();
    }


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
    void visit(PrimaryExpr  &node) override;
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
    
    ErrorReporter reporter;        // для накопления и печати ошибок

    std::vector<std::shared_ptr<Object>> value_stack;
};
