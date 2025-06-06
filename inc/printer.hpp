#pragma once

#include "ast.hpp"
#include <string>

// Класс-посетитель, который обходит узлы AST и собирает строковый результат.
// Здесь мы оставляем ту же самую логику, что и раньше, но добавляем обработку новых узлов (TransUnit.line и ClassDecl).
class ASTPrinterVisitor : public ASTVisitor {
public:
    ASTPrinterVisitor();

    // Возвращает накопленный результат (строку с «раскраской» AST)
    std::string getResult() const;

    // Все методы visit(...) для каждого типа узла
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

    virtual void visit(UnaryExpr &node) override;
    virtual void visit(BinaryExpr &node) override;
    virtual void visit(PrimaryExpr &node) override;
    virtual void visit(TernaryExpr &node) override;
    virtual void visit(IdExpr &node) override;
    virtual void visit(LiteralExpr &node) override;
    virtual void visit(CallExpr &node) override;
    virtual void visit(IndexExpr &node) override;
    virtual void visit(AttributeExpr &node) override;

    virtual void visit(ListExpr &node) override;
    virtual void visit(SetExpr &node) override;
    virtual void visit(DictExpr &node) override;

    void visit(ListComp &node) override;
    void visit(DictComp &node) override;
    void visit(TupleComp &node) override;

    void visit(LambdaExpr &node) override;

    // *** НОВЫЕ МЕТОДЫ ***

    // Печать объявления класса
    virtual void visit(ClassDecl &node) override;

    virtual void visit(class LenStat &node) override;
    virtual void visit(class DirStat &node) override;
    virtual void visit(class EnumerateStat &node) override;

private:
    // Накопленная строка с текстом «раскрашенного» AST
    std::string result;

    // Текущий уровень отступа (0 = без отступа, 1 = 2 пробела, 2 = 4 пробела и т.д.)
    int indentLevel;

    // Возвращает строку из (indentLevel * 2) пробелов
    std::string indent() const;

    // Приписать к result какую-то строку
    void append(const std::string &str);
};
