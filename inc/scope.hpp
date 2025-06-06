#pragma once

#include "symbol_table.hpp"
#include <memory>

class Scope {
public:
    Scope() {
        current = std::make_shared<SymbolTable>(nullptr);
    }

    void enter_scope() {
        current = std::make_shared<SymbolTable>(current);
    }

    void leave_scope() {
        if (auto p = current->find_parent()) {
            current = p;
        }
    }

    bool insert(const Symbol& sym) {
        return current->insert(sym);
    }

    Symbol* lookup(const std::string& name) const {
        return current->lookup(name);
    }

    Symbol* lookup_local(const std::string& name) const {
        return current->lookup_local(name);
    }

    std::shared_ptr<SymbolTable> currentTable() const {
        return current;
    }
    
private:
    std::shared_ptr<SymbolTable> current;
};