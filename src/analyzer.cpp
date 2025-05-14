#include "analyzer.hpp"

SemanticAnalyzer::SemanticAnalyzer() : scopes(), reporter() {}

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