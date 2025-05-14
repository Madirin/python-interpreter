#pragma once

#include "ast.hpp"
#include "scope.hpp"
#include "error_reporter.hpp"
#include <vector>
#include <string>


class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer();

    
    void analyze(TransUnit &unit);


    virtual void visit(TransUnit &node) override;
    virtual void visit(FuncDecl  &node) override;
    virtual void visit(BlockStat &node) override;
    virtual void visit(ExprStat  &node) override;
    virtual void visit(AssignStat&node) override;
    virtual void visit(IdExpr    &node) override;
    virtual void visit(BinaryExpr&node) override;
    virtual void visit(UnaryExpr &node) override;
    virtual void visit(CallExpr  &node) override;
    virtual void visit(IndexExpr &node) override;
    virtual void visit(AttributeExpr &node) override;
    virtual void visit(TernaryExpr  &node) override;
    virtual void visit(LiteralExpr  &node) override;
    virtual void visit(PrimaryExpr  &node) override;
    virtual void visit(ListExpr     &node) override;
    virtual void visit(SetExpr      &node) override;
    virtual void visit(DictExpr     &node) override;

    virtual void visit(CondStat    &node) override;
    virtual void visit(WhileStat   &node) override;
    virtual void visit(ForStat     &node) override;
    virtual void visit(ReturnStat  &node) override;
    virtual void visit(BreakStat   &node) override;
    virtual void visit(ContinueStat&node) override;
    virtual void visit(PassStat    &node) override;
    virtual void visit(AssertStat  &node) override;
    virtual void visit(ExitStat    &node) override;
    virtual void visit(PrintStat   &node) override;

private:
    Scope scopes;
    ErrorReporter reporter;
};
