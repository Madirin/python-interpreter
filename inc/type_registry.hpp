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

    
    void registerType(const std::type_index &ti) {
        if (tables.find(ti) == tables.end()) {
            tables[ti] = {};
        }
    }

    
    void registerMethod(const std::type_index &ti, const std::string &name, Method fn) {
        registerType(ti);
        tables[ti][name] = std::move(fn);
    }

    
    Method* getMethod(const std::type_index &ti, const std::string &name) {
        auto it = tables.find(ti);
        if (it == tables.end()) return nullptr;
        auto jt = it->second.find(name);
        if (jt == it->second.end()) return nullptr;
        return &jt->second;
    }

    
    void registerBuiltins() {
        for (auto &typeEntry : builtinMethodsMap) {
            const std::type_index &ti = typeEntry.first;
            registerType(ti);
            for (auto &m : typeEntry.second) {
                registerMethod(ti, m.first, m.second);
            }
        }
    }

private:
    TypeRegistry() {}


    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    
    std::unordered_map<std::type_index, std::unordered_map<std::string, Method>> tables;

    std::unordered_map<std::type_index, std::vector<std::pair<std::string, Method>> > builtinMethodsMap = {
        
        { typeid(PyInt), {
            { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
        }},

        
        { typeid(PyBool), {
            { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
        }},

        
        { typeid(PyFloat), {
            { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
        }},

        
        { typeid(PyString), {
            { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
            { "__contains__", [](ObjectPtr self, auto &args){
                  bool ok = self->__contains__(args[0]);
                  return std::make_shared<PyBool>(ok);
              }
            },
        }},

        
        { typeid(PyList), {
            { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
            { "__contains__", [](ObjectPtr self, auto &args){
                  bool ok = self->__contains__(args[0]);
                  return std::make_shared<PyBool>(ok);
              }
            },
        }},

        
        { typeid(PyDict), {
            { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
            { "__contains__", [](ObjectPtr self, auto &args){
                  bool ok = self->__contains__(args[0]);
                  return std::make_shared<PyBool>(ok);
              }
            },
        }},

        
        { typeid(PySet), {
            { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
            { "__contains__", [](ObjectPtr self, auto &args){
                  bool ok = self->__contains__(args[0]);
                  return std::make_shared<PyBool>(ok);
              }
            },
        }},
    };
};
