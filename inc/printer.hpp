#pragma once

#include "ast.hpp"
#include <string>

class ASTPrinterVisitor : public ASTVisitor {
public:
    ASTPrinterVisitor();

    
    std::string getResult() const;

    
    virtual void visit(TransUnit &node) override;
    virtual void visit(FuncDecl &node) override;
    virtual void visit(BlockStat &node) override;
    virtual void visit(ExprStat &node) override;
    virtual void visit(CondStat &node) override;
    virtual void visit(WhileStat &node) override;
    virtual void visit(ForStat &node) override;
    virtual void visit(ReturnStat &node) override;
    virtual void visit(BreakStat &node) override;
    virtual void visit(ContinueStat &node) override;
    virtual void visit(PassStat &node) override;
    virtual void visit(AssertStat &node) override;
    virtual void visit(ExitStat &node) override;
    virtual void visit(PrintStat &node) override; 
    virtual void visit(AssignStat &node) override;

    
    virtual void visit(OrExpr &node) override;
    virtual void visit(AndExpr &node) override;
    virtual void visit(NotExpr &node) override;
    virtual void visit(ComparisonExpr &node) override;
    virtual void visit(ArithExpr &node) override;
    virtual void visit(TermExpr &node) override;
    virtual void visit(FactorExpr &node) override;
    virtual void visit(PowerExpr &node) override;
    virtual void visit(PrimaryExpr &node) override;
    virtual void visit(TernaryExpr &node) override;
    virtual void visit(IdExpr &node) override;
    virtual void visit(LiteralExpr &node) override;
    virtual void visit(CallExpr &node) override;
    virtual void visit(IndexExpr &node) override;
    virtual void visit(AttributeExpr &node) override;
    
private:
    std::string result;
    int indentLevel;

    
    std::string indent() const;
   
    void append(const std::string &str);
};
