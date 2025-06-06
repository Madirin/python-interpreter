#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>

class Object;       
struct ASTNode;

enum class SymbolType {
    Variable, Parameter, Function, BuiltinFunction, UserClass
};

struct Symbol {
    std::string name;
    SymbolType type = SymbolType::Variable;
    std::shared_ptr<Object> value;
    ASTNode* decl = nullptr;
};

class SymbolTable {
public:
    SymbolTable(std::shared_ptr<SymbolTable> parent = nullptr) : parent(std::move(parent)) {}

    bool insert(const Symbol& sym) {
        if (table.find(sym.name) != table.end()) {
            return false;
        }

        table[sym.name] = sym;
        return true;
    }

    Symbol* lookup_local(const std::string& name) const {
        auto it = table.find(name);
        if (it != table.end()) {
            return const_cast<Symbol*>(&it->second);
        }
        return nullptr;
    }

    Symbol* lookup(const std::string& name) const {
        if (auto *local = lookup_local(name)) {
            return local;
        }
        if (parent) {
            return parent->lookup(name);
        }
        return nullptr;
    }

    std::shared_ptr<SymbolTable> find_parent() const {
        return parent;
    }
private:
    std::unordered_map<std::string, Symbol> table;
    std::shared_ptr<SymbolTable> parent;
};