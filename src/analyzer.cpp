#include "analyzer.hpp"

SemanticAnalyzer::SemanticAnalyzer() : scopes(), reporter() {

    scopes.insert(Symbol{"range", SymbolType::Function, nullptr, ""});
    scopes.insert(Symbol{"print", SymbolType::Function, nullptr, ""});
}


std::string SemanticAnalyzer::giveme_type(Expression *expr) const {
    
    if (auto lit = dynamic_cast<LiteralExpr*>(expr)) {
        if (std::holds_alternative<int>(lit->value)) return "int";
        if (std::holds_alternative<double>(lit->value)) return "float";
        if (std::holds_alternative<std::string>(lit->value)) return "str";
        if (std::holds_alternative<bool>(lit->value)) return "bool";
        if (std::holds_alternative<std::monostate>(lit->value)) return "NoneType";
    }

    if (auto id = dynamic_cast<IdExpr*>(expr)) {
        if (auto sym = scopes.lookup(id->name)) {
            return sym->varType.empty() ? "object" : sym->varType;
        }
        return "object";
    }

    if (dynamic_cast<ListExpr*>(expr)) return "list";
    if (dynamic_cast<DictExpr*>(expr)) return "dict";
    if (dynamic_cast<SetExpr*>(expr)) return "set";

    return "object";

}


void SemanticAnalyzer::analyze(TransUnit &unit) {

    unit.accept(*this);

    if (reporter.has_errors()) {
        reporter.print_errors();
    }
}

void SemanticAnalyzer::visit(TransUnit &node) {
    
    for (auto &unit : node.units) {
        unit->accept(*this);
    }
}

void SemanticAnalyzer::visit(FuncDecl &node) {


    if (auto *existing = scopes.lookup_local(node.name)) {
        existing->type = SymbolType::Function;
        existing->decl = &node;
    } else {
        Symbol sym;
        sym.name = node.name;
        sym.type = SymbolType::Function;
        sym.decl = &node;
        scopes.insert(sym);
    }

    scopes.enter_scope();

    for (auto &p : node.posParams) {
        Symbol param_sym{p, SymbolType::Parameter};
        if (scopes.lookup_local(param_sym.name)) {
            reporter.add_error("Линия " + std::to_string(node.line) + ": дупликат аргумент '" + param_sym.name + "' в определении функции '" + node.name);
        } else {
            scopes.insert(param_sym);
        }
    }

    for (auto &d : node.defaultParams) {
        const std::string &d_name = d.first;
        Symbol param_sym{d_name, SymbolType::Parameter};
        if (scopes.lookup_local(param_sym.name)) {
            reporter.add_error("Линия " + std::to_string(node.line) + ": дупликат аргумент '" + param_sym.name + "' в определении функции '" + node.name);
        } else {
            scopes.insert(param_sym);
        }
        if (d.second) {
            d.second->accept(*this);
        }
    }

    if (node.body) {
        node.body->accept(*this);
    }

    scopes.leave_scope();
}

void SemanticAnalyzer::visit(BlockStat &node) {

    for (auto &stat : node.statements) {
        stat->accept(*this);
    }
}

void SemanticAnalyzer::visit(ExprStat &node) {

    if (node.expr) {
        node.expr->accept(*this);
    }
}

void SemanticAnalyzer::visit(AssignStat &node) {

    if (auto id = dynamic_cast<IdExpr*>(node.left.get())) {

        if(!scopes.lookup_local(id->name)) {
            Symbol sym;
            sym.name = id->name;
            sym.type = SymbolType::Variable;

            sym.decl = &node;
            scopes.insert(sym);
        }
        // если есть, то просто заменяется
    }

    else if (auto attr = dynamic_cast<AttributeExpr*>(node.left.get())) {
        attr->accept(*this);
    }

    else if (auto idx = dynamic_cast<IndexExpr*>(node.left.get())) {
        idx->accept(*this);
    }

    else {
        reporter.add_error("Линия " + std::to_string(node.line) + ": Недопустимое назначение (AssignStat error)");
    }

    if (node.right) {
        node.right->accept(*this);
    }

    if (auto id = dynamic_cast<IdExpr*>(node.left.get())) {
        Symbol *sym = scopes.lookup_local(id->name);
        if (sym) {
            sym->varType = giveme_type(node.right.get());
        }
    }
}

void SemanticAnalyzer::visit(IdExpr &node) {

    Symbol *sym = scopes.lookup(node.name);

    if (!sym) {
        reporter.add_error("Line " + std::to_string(node.line) + ": name '" + node.name + "' is not defined");
    }
}



void SemanticAnalyzer::visit(BinaryExpr &node) {

    if (node.left) {
        node.left->accept(*this);
    }

    if (node.right) {
        node.right->accept(*this);
    }

    std::string left_t = giveme_type(node.left.get());
    std::string right_t = giveme_type(node.right.get());

    if (node.op == "in" || node.op == "not in") {

        bool iterable = (right_t == "list" || right_t == "dict" || right_t == "set" || right_t == "str");

        if (!iterable) {
                reporter.add_error(
                    "line " + std::to_string(node.line) + " TypeError: argument of type '" + right_t + "' is not iterable"
                );
        }

        return;
    }

    if (node.op == "+" || node.op == "-") {
        bool left_num = (left_t == "int" || left_t == "float");
        bool right_num = (right_t == "int" || right_t == "float");
        bool left_str = (left_t == "str");
        bool right_str = (right_t == "str");


        if (node.op != "+") {
            if (!((left_num && right_num) || (left_str && right_str))) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: unsupported operand type(s) for " + node.op + ": '" + 
                left_t + "' and '" + right_t + "'"
                );
            }
        }
        else {
            if (!(left_num && right_num) || (left_str && right_str)) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: unsupported operand type(s) for " + node.op + ": '" + 
                left_t + "' and '" + right_t + "'"
                );
            }
        }
    }
}

void SemanticAnalyzer::visit(UnaryExpr &node) {

    if (node.operand) {
        node.operand->accept(*this);
    }

    // not у нас хороший

    std::string t = giveme_type(node.operand.get());

    if (node.op == "+" || node.op == "-") {
        if (!(t == "int" || t == "float")) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: bad operand type for unary " + node.op + ": '" + t + "'"
                );
        }
        return;
    }

    reporter.add_error("line " + std::to_string(node.line) + " SyntaxError: invalid syntax '" + node.op + "'");
}


void SemanticAnalyzer::visit(CallExpr &node) {
    
    node.caller->accept(*this);

    for (auto &arg : node.arguments) {
        arg->accept(*this);
    }

    if (auto id = dynamic_cast<IdExpr*>(node.caller.get())) {
        Symbol *sym = scopes.lookup(id->name);

        if (!sym) {

            reporter.add_error(
                "Line " + std::to_string(node.line)
                + ": name '" + id->name + "' is not defined"
            );
            return;
        }

        if (sym->type != SymbolType::Function) {
            reporter.add_error(
            "Line " + std::to_string(node.line) + " TypeError: '" + id->name + "' object `" + sym->varType + "` is not callable"
            );
            return;
        }

        if (sym->decl == nullptr) {
            return;
        }

        auto funcdecl = dynamic_cast<FuncDecl*>(sym->decl);

        size_t pos_params = funcdecl->posParams.size();
        size_t def_params = funcdecl->defaultParams.size();
        size_t input = node.arguments.size();

        if (input < pos_params) {
            size_t miss_count = pos_params - input;

            std::vector<std::string> miss_names;

            for (size_t i = input; i < pos_params; i++) {
                miss_names.push_back("'" + funcdecl->posParams[i] + "'");
            }

            std::string total;

            if (miss_count == 1) {
                total = miss_names[0];
            } else {
                for (size_t i = 0; i < miss_names.size(); i++) {
                    total += miss_names[i];
                    if (i + 1 < miss_names.size()) total += " and ";
                }
            }

            reporter.add_error(
                "Line " + std::to_string(node.line) + " TypeError: " + funcdecl->name + "() missing " + std::to_string(miss_count)
                + " required positional argument: " + total);
        }

        if (input > pos_params + def_params) {
            reporter.add_error(
                "Line " + std::to_string(node.line) +
                "TypeError: " + funcdecl->name + "() takes from " + std::to_string(pos_params) + " to " 
                + std::to_string(pos_params + def_params) + " positional arguments but " + std::to_string(input) + " were given"
            );
        }
    }
}

void SemanticAnalyzer::visit(IndexExpr &node) {

    node.base->accept(*this);
    node.index->accept(*this);

    auto type_error = [&](const std::string &msg) {
        reporter.add_error(
            "Line " + std::to_string(node.line) + " TypeError: " + msg
        );
    };

    if (auto id = dynamic_cast<IdExpr*>(node.base.get())) {
        Symbol *sym = scopes.lookup(id->name);
        if (sym) {
            const std::string &t = sym->varType;
            if (t == "str") {
                if (auto idxlit = dynamic_cast<LiteralExpr*>(node.index.get())) {
                    if (!std::holds_alternative<int>(idxlit->value)) {
                        type_error("string indices must be integers");
                    }
                }

                return; // runtime thing
            }

            if (t == "list") {
                if (auto idxlit = dynamic_cast<LiteralExpr*>(node.index.get())) {
                    if (!std::holds_alternative<int>(idxlit->value)) {
                            type_error("list indices must be integers");
                    }
                }
                return; // runtime thing
            }

            if (t == "dict") {
                return;
            }

            if (t == "set") {
                type_error("'set' object is not subscriptable");
                return;
            }
        }

        type_error("'" + giveme_type(node.base.get()) + "' object is not subscriptable");
    }

}


void SemanticAnalyzer::visit(AttributeExpr &node) {
    
    if (node.obj) {
        node.obj->accept(*this);
    }

    // str, int, bool, none. float no attrs
    if (auto id = dynamic_cast<IdExpr*>(node.obj.get())) {
        Symbol *sym = scopes.lookup(id->name);
        if (sym) {
            const std::string &t = sym->varType;
            if (t == "str" || t == "int" || t == "bool" || t == "NoneType" || t == "float") {
                reporter.add_error(
                "Line " + std::to_string(node.line) +
                " TypeError: '" + sym->varType +
                "' object has no attribute '" + node.name + "'"
                );
            } else {
                return;
            }
        }
    }
}

void SemanticAnalyzer::visit(TernaryExpr &node) {

    if (node.condition) {
        node.condition->accept(*this);
    }

    if (node.trueExpr) {
        node.trueExpr->accept(*this);
    }

    if (node.falseExpr) {
        node.falseExpr->accept(*this);
    }

}


void SemanticAnalyzer::visit(LiteralExpr &node) {}

void SemanticAnalyzer::visit(PrimaryExpr &node) {

    switch (node.type) {
        case PrimaryExpr::PrimaryType::LITERAL:
            if (node.literalExpr) {
                node.literalExpr->accept(*this);
            }
            break;

        case PrimaryExpr::PrimaryType::ID:
            if (node.idExpr) {
                node.idExpr->accept(*this);
            }
            break;

        case PrimaryExpr::PrimaryType::CALL:
            if (node.callExpr) {
                node.callExpr->accept(*this);
            }
            break;

        case PrimaryExpr::PrimaryType::INDEX:
            if (node.indexExpr) {
                node.indexExpr->accept(*this);
            }
            break;

        case PrimaryExpr::PrimaryType::PAREN:
            if (node.parenExpr) {
                node.parenExpr->accept(*this);
            }
            break;

        case PrimaryExpr::PrimaryType::TERNARY:
            if (node.ternaryExpr) {
                node.ternaryExpr->accept(*this);
            }
            break;
    }
}


void SemanticAnalyzer::visit(ListExpr &node) {

    for (auto &elem : node.elems) {
        if (elem) {
            elem->accept(*this);
        }
    }
}

void SemanticAnalyzer::visit(DictExpr &node) {

    for (auto &pair : node.items) {

        if (pair.first) {
            pair.first->accept(*this);

            if (dynamic_cast<ListExpr*>(pair.first.get()) || dynamic_cast<DictExpr*>(pair.first.get()) || dynamic_cast<SetExpr*>(pair.first.get())) {
                reporter.add_error(
                "Line " + std::to_string(node.line) + " TypeError: unhashable type: '" + giveme_type(pair.first.get()) + "'"
                );
            }
        }

        if (pair.second) {
            pair.second->accept(*this);
        }
    }
}

void SemanticAnalyzer::visit(SetExpr &node) {

    for (auto &elem : node.elems) {
        if (elem) {
            elem->accept(*this);
        }

        if (dynamic_cast<ListExpr*>(elem.get()) || dynamic_cast<DictExpr*>(elem.get()) || dynamic_cast<SetExpr*>(elem.get())) {
            reporter.add_error(
                "Line " + std::to_string(node.line) + " TypeError: unhashable type: '" + giveme_type(elem.get()) + "'"
            );
        }
    }
}

void SemanticAnalyzer::visit(CondStat &node) {

    if (node.condition) {
        node.condition->accept(*this);
    }


    if (node.ifblock) {
        node.ifblock->accept(*this);
    }


    for (auto &elifPair : node.elifblocks) {
        if (elifPair.first) {
            elifPair.first->accept(*this);
        }
        if (elifPair.second) {
            elifPair.second->accept(*this);
        }
    }


    if (node.elseblock) {
        node.elseblock->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStat &node) {

    if (node.condition) {
        node.condition->accept(*this);
    }

    if (node.body) {
        node.body->accept(*this);
    }
}

void SemanticAnalyzer::visit(ForStat &node) {

    if (node.iterable) {
        node.iterable->accept(*this);
    }

    if (auto lit = dynamic_cast<LiteralExpr*>(node.iterable.get())) {
        if (std::holds_alternative<int>(lit->value) || std::holds_alternative<double>(lit->value) ||
            std::holds_alternative<bool>(lit->value) || std::holds_alternative<std::monostate>(lit->value)) {

                reporter.add_error(
                    "Line " + std::to_string(lit->line) + " TypeError: '" + giveme_type(lit) + "' object is not iterable"
                );
        }
    }

    for (auto &var : node.iterators) {
        if (!scopes.lookup_local(var)) {
            Symbol sym{var, SymbolType::Variable, nullptr};
            scopes.insert(sym);
        }
    }

    if (node.body) {
        node.body->accept(*this);
    }
} 


void SemanticAnalyzer::visit(ReturnStat &node) {

    if (node.expr) {
        node.expr->accept(*this);
    }

}

void SemanticAnalyzer::visit(BreakStat &node) {
}

void SemanticAnalyzer::visit(ContinueStat &node) {
}

void SemanticAnalyzer::visit(PassStat &node) {
}

void SemanticAnalyzer::visit(AssertStat &node) {

    if (node.condition) {
        node.condition->accept(*this);
    }
}

void SemanticAnalyzer::visit(ExitStat &node) {

    if (node.expr) {
        node.expr->accept(*this);
    }
}

void SemanticAnalyzer::visit(PrintStat &node) {
    if (node.expr) {
        node.expr->accept(*this);
    }
}

