#include "executor.hpp"
#include "type_registry.hpp"
#include "builtin_function.hpp"  
#include "object.hpp"
#include <memory>
#include <iostream>
 
Executor::Executor() : scopes(), reporter() {
    
    TypeRegistry::instance().registerBuiltins();

    auto print_fn = std::make_shared<PyBuiltinFunction>(
        "print", [](const std::vector<ObjectPtr>& args) {
            for (auto &a : args) {
                std::cout << a->repr() << " ";
                std::cout << "\n";
                return std::make_shared<PyNone>();
            }
        }
    );
    scopes.insert(Symbol{"print", SymbolType::BuiltinFunction, print_Fn, nullptr});

    auto range_fn = std::make_shared<PyBuiltinFunction>(
        "range", [](const std::vector<ObjectPtr>& args) {
            int n = std::dynamic_pointer_cast<PyInt>(args[0])->get();
            std::vector<ObjectPtr> elems;
            for (int i = 0; i < n; i++) {
                elems.push_back(std::make_shared<PyInt>(i));
            }
            return std::make_shared<PyList>(std::move(elems));
        }
    );
    scopes.insert(Symbol{"range", SymbolType::BuiltinFunction, range_fn, nullptr});
}

std::shared_ptr<Object> Executor::evaluate(Expression &expr) {
    expr.accept(*this);
    return getValue();
}

void Executor::execute(TransUnit &unit) {
    try {
        unit.accept(*this);
    }
    catch (const RuntimeError &err) {
        std::cerr << "RuntimeError: " << err.what() << "\n";
        return;
    }

    if (reporter.has_errors()) {
        reporter.print_errors();
    }
}

void Executor::visit(TransUnit &node) {
    for (auto &unit : node.units) {
        unit->accept(*this);
    }
}

void Executor::visit(FuncDecl &node) {
    // 1) Проверяем дупликаты имён параметров
    {
        std::unordered_set<std::string> seen;

        // позиционные
        for (const auto &name : node.posParams) {
            if (!seen.insert(name).second) {
                reporter.add_error(
                    "Line " + std::to_string(node.line)
                    + ": duplicate parameter name '" + name
                    + "' in function '" + node.name + "'"
                );
            }
        }
        // с default-значением
        for (const auto &pair : node.defaultParams) {
            const std::string &name = pair.first;
            if (!seen.insert(name).second) {
                reporter.add_error(
                    "Line " + std::to_string(node.line)
                    + ": duplicate parameter name '" + name
                    + "' in function '" + node.name + "'"
                );
            }
        }
    }

    // 2) Если были ошибки — сразу отваливаемся
    if (reporter.has_errors()) {
        return;
    }

    // 3) Вычисляем default-значения в текущем окружении
    std::vector<ObjectPtr> default_values;
    default_values.reserve(node.defaultParams.size());
    
    for (auto &pair : node.defaultParams) {
        Expression *expr = pair.second.get();
        if (expr) {
            expr->accept(*this);
            default_values.push_back(popValue());
        } else {
            default_values.push_back(std::make_shared<PyNone>());
        }
    }

    // 4) Создаём объект функции, захватывая ТЕКУЩЕЕ ОКРУЖЕНИЕ как замыкание
    auto fn_obj = std::make_shared<PyUserFunction>(
        node.name,
        &node,
        scopes.currentTable(),    // вот это наша "environment" для замыкания
        node.posParams,
        std::move(default_values)
    );

    // 5) Кладём этот объект в таблицу символов _ТЕКУЩЕЙ_ области
    Symbol sym;
    sym.name  = node.name;
    sym.type  = SymbolType::UserFunction;
    sym.value = fn_obj;
    sym.decl  = &node;

    if (auto existing = scopes.lookup_local(node.name)) {
        *existing = sym;
    } else {
        scopes.insert(sym);
    }

    // Никакого leave_scope() — мы не входили никуда
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

