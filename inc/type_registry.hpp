#pragma once

#include <typeindex>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include "object.hpp"

using Method = std::function<ObjectPtr(ObjectPtr, const std::vector<ObjectPtr>&)>;


class TypeRegistry {
public:
    
    static TypeRegistry& instance() {
        static TypeRegistry inst;
        return inst;
    }

    
    TypeRegistry& registerType(const std::type_index &ti) {
        if (tables.find(ti) == tables.end()) {
            tables[ti] = {};
        }
        return *this;
    }

    TypeRegistry& registerMethod(const std::type_index &ti, const std::string &name, Method fn) {
        registerType(ti);
        tables[ti][name] = std::move(fn);
        return *this;
    }

    Method* getMethod(const std::type_index &ti, const std::string &name) {
        auto it = tables.find(ti);
        
        if (it == tables.end()) {
            return nullptr;
        }

        auto jt = it->second.find(name);
        if (jt == it->second.end()) {
            return nullptr;
        }

        return &jt->second;
    }
    
    std::shared_ptr<SymbolTable> getTable(const std::type_index &t) {
        registerType(t);
        return tables[t];
    }

private:
    
    TypeRegistry() = default;
    
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;
    
    std::unordered_map<std::type_index, std::shared_ptr<SymbolTable>> tables;
};
