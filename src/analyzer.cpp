#include "analyzer.hpp"

SemanticAnalyzer::SemanticAnalyzer() : scopes(), reporter() {}


std::string SemanticAnalyzer::giveme_type(Expression *expr) const {
    
    if (auto lit = dynamic_cast<LiteralExpr*>(expr)) {
        if (std::holds_alternative<int>(lit->value)) return "int";
        if (std::holds_alternative<double>(lit->value)) return "float";
        if (std::holds_alternative<std::string>(lit->value)) return "str";
        if (std::holds_alternative<bool>(lit->value)) return "bool";
        if (std::holds_alternative<std::monostate>(lit->value)) return "NoneType";
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

    Symbol sym;
    sym.name = node.name;
    sym.type = SymbolType::Function;

    if (auto *existing = scopes.lookup_local(node.name)) {
        existing->type = SymbolType::Function;
        existing->decl = &node;
    } else {
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
        attr->obj->accept(*this);
    }

    else if (auto idx = dynamic_cast<IndexExpr*>(node.left.get())) {
        idx->base->accept(*this);
        idx->index->accept(*this);
    }

    else {
        reporter.add_error("Линия " + std::to_string(node.line) + ": Недопустимое назначение (AssignStat error)");
    }

    if (node.right) {
        node.right->accept(*this);
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

    if (node.op == "in" || node.op == "not in") {

        bool iterable = (dynamic_cast<ListExpr*>(node.right.get()) ||
                        dynamic_cast<DictExpr*>(node.right.get()) ||
                        dynamic_cast<SetExpr*>(node.right.get()));

        if (!iterable) {
            if (auto lit = dynamic_cast<LiteralExpr*>(node.right.get())) {
                if (std::holds_alternative<std::string>(lit->value)) {
                    return;
                } 
                else {
                    reporter.add_error(
                    "line " + std::to_string(node.line) + " TypeError: argument of type '" + giveme_type(node.right.get()) + "' is not iterable"
                    );
                }
            }
        }
        return;
    }

    if (node.op == "+" || node.op == "-") {
        bool left_num, right_num, left_str, right_str;

        if (auto lit = dynamic_cast<LiteralExpr*>(node.left.get())) {
            if (std::holds_alternative<int>(lit->value) || std::holds_alternative<double>(lit->value)) {
                left_num = true;
                left_str = false;
            } 
            else if (std::holds_alternative<std::string>(lit->value)) {
                left_num = false;
                left_str = true;
            }
        }

        if (auto lit = dynamic_cast<LiteralExpr*>(node.right.get())) {
            if (std::holds_alternative<int>(lit->value) || std::holds_alternative<double>(lit->value)) {
                right_num = true;
                right_str = false;
            } 
            else if (std::holds_alternative<std::string>(lit->value)) {
                right_num = false;
                right_str = true;
            }
        }

        if (node.op != "-") {
            if (!((left_num && right_num) || (left_str && right_str))) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: unsupported operand type(s) for " + node.op + ": '" + 
                giveme_type(node.left.get()) + "' and '" + giveme_type(node.right.get()) + "'"
                );
            }
        }
        else {
            if (!(left_num && right_num) || (left_str && right_str)) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: unsupported operand type(s) for " + node.op + ": '" + 
                giveme_type(node.left.get()) + "' and '" + giveme_type(node.right.get()) + "'"
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

    if (node.op == "+" || node.op == "-") {
        if (auto lit = dynamic_cast<LiteralExpr*>(node.operand.get())) {
            bool is_string = std::holds_alternative<std::string>(lit->value);
            bool is_none = std::holds_alternative<std::monostate>(lit->value);
            if (is_string || is_none) {
                reporter.add_error(
                "line " + std::to_string(node.line) + " TypeError: bad operand type for unary " + node.op + ": '" + 
                giveme_type(node.operand.get()) + "'"
                );
            }
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

        // node.caller->accept проверит сущестование имени

        if (sym->type != SymbolType::Function) {
            reporter.add_error(
            "Line " + std::to_string(node.line) + " TypeError: '" + id->name + "' object is not callable"
            );
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
                + std::to_string(def_params) + " positional arguments but " + std::to_string(input) + " were given"
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

    if (auto lit = dynamic_cast<LiteralExpr*>(node.base.get())) {
        if (std::holds_alternative<std::string>(lit->value)) {
            if (auto idxlit = dynamic_cast<LiteralExpr*>(node.index.get())) {
                if (!std::holds_alternative<int>(idxlit->value)) {
                    type_error("string indices must be integers");
                }
            }

            return; // runtime thing
        }

        type_error("'" + giveme_type(node.base.get()) + "' object is not subscriptable");
    }

    if (dynamic_cast<ListExpr*>(node.base.get())) {
        if (auto idxlit = dynamic_cast<LiteralExpr*>(node.index.get())) {
            if (!std::holds_alternative<int>(idxlit->value)) {
                    type_error("list indices must be integers");
            }
        }
        return; // runtime thing
    }

    if (dynamic_cast<DictExpr*>(node.base.get())) {
        return;
    }

    if (dynamic_cast<SetExpr*>(node.base.get())) {
        type_error("'set' object is not subscriptable");
        return;
    }
}