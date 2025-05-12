#include "printer.hpp"
#include <sstream>
#include <variant>

ASTPrinterVisitor::ASTPrinterVisitor() : indentLevel(0) {}

std::string ASTPrinterVisitor::getResult() const {
    return result;
}

std::string ASTPrinterVisitor::indent() const {
    return std::string(indentLevel * 2, ' '); // 2 пробела на уровень
}

void ASTPrinterVisitor::append(const std::string &str) {
    result += str;
}


void ASTPrinterVisitor::visit(TransUnit &node) {
    append("TransUnit:\n");
    indentLevel++;
    for (auto &unit : node.units) {
        unit->accept(*this);
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(FuncDecl &node) {
    append(indent() + "FuncDecl: " + node.name + "(");
    for (size_t i = 0; i < node.params.size(); i++) {
        append(node.params[i]);
        if (i + 1 < node.params.size()) {
            append(", ");
        }
    }
    append(")\n");
    indentLevel++;
    if (node.body) {
        node.body->accept(*this);
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(BlockStat &node) {
    append(indent() + "BlockStat:\n");
    indentLevel++;
    for (auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(ExprStat &node) {
    append(indent() + "ExprStat:\n");
    indentLevel++;
    if (node.expr)
        node.expr->accept(*this);
    indentLevel--;
}


void ASTPrinterVisitor::visit(CondStat &node) {
    append(indent() + "CondStat:\n");
    indentLevel++;
    append(indent() + "If condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;
    append(indent() + "If block:\n");
    indentLevel++;
    node.ifblock->accept(*this);
    indentLevel--;
    for (auto &elifPair : node.elifblocks) {
        append(indent() + "Elif condition:\n");
        indentLevel++;
        elifPair.first->accept(*this);
        indentLevel--;
        append(indent() + "Elif block:\n");
        indentLevel++;
        elifPair.second->accept(*this);
        indentLevel--;
    }
    if (node.elseblock) {
        append(indent() + "Else block:\n");
        indentLevel++;
        node.elseblock->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(WhileStat &node) {
    append(indent() + "WhileStat:\n");
    indentLevel++;
    append(indent() + "Condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;
    append(indent() + "Body:\n");
    indentLevel++;
    node.body->accept(*this);
    indentLevel -= 2;
}


void ASTPrinterVisitor::visit(ForStat &node) {
    append(indent() + "ForStat: iterator = " + node.iterator + "\n");
    indentLevel++;
    append(indent() + "Iterable:\n");
    indentLevel++;
    node.iterable->accept(*this);
    indentLevel--;
    append(indent() + "Body:\n");
    indentLevel++;
    node.body->accept(*this);
    indentLevel -= 2;
}


void ASTPrinterVisitor::visit(ReturnStat &node) {
    append(indent() + "ReturnStat:\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}


void ASTPrinterVisitor::visit(BreakStat &node) {
    append(indent() + "BreakStat\n");
}


void ASTPrinterVisitor::visit(ContinueStat &node) {
    append(indent() + "ContinueStat\n");
}


void ASTPrinterVisitor::visit(PassStat &node) {
    append(indent() + "PassStat\n");
}


void ASTPrinterVisitor::visit(AssertStat &node) {
    append(indent() + "AssertStat:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;
}


void ASTPrinterVisitor::visit(ExitStat &node) {
    append(indent() + "ExitStat:\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}


void ASTPrinterVisitor::visit(PrintStat &node) {
    append(indent() + "PrintStat:\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}

void ASTPrinterVisitor::visit(AssignStat &node) {
    append(indent() + "AssignStat:\n");
    indentLevel++;
    
    append(indent() + "Left:\n");
    indentLevel++;
    
    if (node.left) {
        node.left->accept(*this);
    }
    indentLevel--;
    
    append(indent() + "Right:\n");
    indentLevel++;
    if (node.right) {
        node.right->accept(*this);
    }
    indentLevel--;
    
    
    indentLevel--;
}

// Visitor для OrExpr
void ASTPrinterVisitor::visit(OrExpr &node) {
    append(indent() + "OrExpr:\n");
    indentLevel++;
    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;
    for (auto &pair : node.rights) {
        append(indent() + "Operator: " + pair.first + "\n");
        append(indent() + "Right:\n");
        indentLevel++;
        pair.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}

// Visitor для AndExpr
void ASTPrinterVisitor::visit(AndExpr &node) {
    append(indent() + "AndExpr:\n");
    indentLevel++;
    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;
    for (auto &pair : node.rights) {
        append(indent() + "Operator: " + pair.first + "\n");
        append(indent() + "Right:\n");
        indentLevel++;
        pair.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(NotExpr &node) {
    append(indent() + "NotExpr:\n");
    indentLevel++;
    node.comparison->accept(*this);
    indentLevel--;
}


void ASTPrinterVisitor::visit(ComparisonExpr &node) {
    append(indent() + "ComparisonExpr:\n");
    indentLevel++;
    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;
    for (auto &pair : node.rights) {
        append(indent() + "Operator: " + pair.first + "\n");
        append(indent() + "Right:\n");
        indentLevel++;
        pair.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(ArithExpr &node) {
    append(indent() + "ArithExpr:\n");
    indentLevel++;
    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;
    for (auto &pair : node.rights) {
        append(indent() + "Operator: " + pair.first + "\n");
        append(indent() + "Right:\n");
        indentLevel++;
        pair.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(TermExpr &node) {
    append(indent() + "TermExpr:\n");
    indentLevel++;
    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;
    for (auto &pair : node.rights) {
        append(indent() + "Operator: " + pair.first + "\n");
        append(indent() + "Right:\n");
        indentLevel++;
        pair.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}


void ASTPrinterVisitor::visit(FactorExpr &node) {
    append(indent() + "FactorExpr: " + node.unaryOp + "\n");
    indentLevel++;
    node.operand->accept(*this);
    indentLevel--;
}


void ASTPrinterVisitor::visit(PowerExpr &node) {
    append(indent() + "PowerExpr:\n");
    indentLevel++;
    append(indent() + "Base:\n");
    indentLevel++;
    node.base->accept(*this);
    indentLevel--;
    if (node.exponent) {
        append(indent() + "Exponent:\n");
        indentLevel++;
        node.exponent->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}

void ASTPrinterVisitor::visit(PrimaryExpr &node) {
    append(indent() + "PrimaryExpr: ");
    switch (node.type) {
        case PrimaryExpr::PrimaryType::LITERAL:
            append("Literal\n");
            if (node.literalExpr) {
                indentLevel++;
                node.literalExpr->accept(*this);
                indentLevel--;
            }
            break;
        case PrimaryExpr::PrimaryType::ID:
            append("Identifier\n");
            if (node.idExpr) {
                indentLevel++;
                node.idExpr->accept(*this);
                indentLevel--;
            }
            break;
        case PrimaryExpr::PrimaryType::CALL:
            append("Call\n");
            if (node.callExpr) {
                indentLevel++;
                node.callExpr->accept(*this);
                indentLevel--;
            }
            break;
        case PrimaryExpr::PrimaryType::INDEX:
            append("Index\n");
            if (node.indexExpr) {
                indentLevel++;
                node.indexExpr->accept(*this);
                indentLevel--;
            }
            break;
        case PrimaryExpr::PrimaryType::PAREN:
            append("Parenthesized Expression\n");
            if (node.parenExpr) {
                indentLevel++;
                node.parenExpr->accept(*this);
                indentLevel--;
            }
            break;
        case PrimaryExpr::PrimaryType::TERNARY:
            append("Ternary Expression\n");
            if (node.ternaryExpr) {
                indentLevel++;
                node.ternaryExpr->accept(*this);
                indentLevel--;
            }
            break;
        default:
            append("Unknown PrimaryExpr Type\n");
    }
}


void ASTPrinterVisitor::visit(TernaryExpr &node) {
    append(indent() + "TernaryExpr:\n");
    indentLevel++;
    append(indent() + "TrueExpr:\n");
    indentLevel++;
    node.trueExpr->accept(*this);
    indentLevel--;
    append(indent() + "Condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;
    append(indent() + "FalseExpr:\n");
    indentLevel++;
    node.falseExpr->accept(*this);
    indentLevel--;
    indentLevel--;
}


void ASTPrinterVisitor::visit(IdExpr &node) {
    append(indent() + "IdExpr: " + node.name + "\n");
}


void ASTPrinterVisitor::visit(LiteralExpr &node) {
    std::string typeStr;
    std::string valStr;

    // switch(node.literalType) {
    //     case LiteralExpr::LiteralType::INT: typeStr = "INT"; break;
    //     case LiteralExpr::LiteralType::FLOAT: typeStr = "FLOAT"; break;
    //     case LiteralExpr::LiteralType::STRING: typeStr = "STRING"; break;
    //     case LiteralExpr::LiteralType::BOOL: typeStr = "BOOL"; break;
    //     case LiteralExpr::LiteralType::NONE: typeStr = "NONE"; break;
    // }

    if (std::holds_alternative<int>(node.value)) {
        typeStr = "INT";
        valStr = std::to_string(std::get<int>(node.value));
    } else if (std::holds_alternative<double>(node.value)) {
        typeStr = "FLOAT";
        valStr = std::to_string(std::get<double>(node.value));
    } else if (std::holds_alternative<std::string>(node.value)) {
        typeStr = "STRING";
        valStr = std::get<std::string>(node.value);
    } else if (std::holds_alternative<bool>(node.value)) {
        typeStr = "BOOL";
        valStr = (std::get<bool>(node.value) ? "True" : "False");
    } else if (std::holds_alternative<std::monostate>(node.value)) {
        typeStr = "NONE";
        valStr = "None";
    }

    append(indent() + "LiteralExpr: [" + typeStr + "] " + valStr + "\n");
}


void ASTPrinterVisitor::visit(CallExpr &node) {
    append(indent() + "CallExpr:\n");
    indentLevel++;
    append(indent() + "Caller:\n");
    indentLevel++;
    node.caller->accept(*this);
    indentLevel--;
    append(indent() + "Arguments:\n");
    indentLevel++;
    for (auto &arg : node.arguments) {
        arg->accept(*this);
    }
    indentLevel--;
    indentLevel--;
}


void ASTPrinterVisitor::visit(IndexExpr &node) {
    append(indent() + "IndexExpr:\n");
    indentLevel++;
    append(indent() + "Base:\n");
    indentLevel++;
    node.base->accept(*this);
    indentLevel--;
    append(indent() + "Index:\n");
    indentLevel++;
    node.index->accept(*this);
    indentLevel -= 2;
}

void ASTPrinterVisitor::visit(AttributeExpr &node) {
    append(indent() + "AttributeExpr: ." + node.name + "\n");

    indentLevel++;
    append(indent() + "Object:\n");
    indentLevel++;
    node.obj->accept(*this);
    indentLevel -= 2;
}