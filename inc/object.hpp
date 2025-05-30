#pragma once

#include "ast.hpp"
#include "executor.hpp"
#include <functional>
#include <sstream>
#include <memory>
#include <vector>
#include <typeindex>
#include <string>

class Object;
using ObjectPtr = std::shared_ptr<Object>;

struct RuntimeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Object {
public:
    virtual ~Object() = default;

    virtual std::shared_ptr<Object> __add__(std::shared_ptr<Object> right) {
        throw RuntimeError("unsupported operand types for +");
    }

    virtual std::shared_ptr<Object> __sub__(std::shared_ptr<Object> right) {
        throw RuntimeError("unsupported operand types for -");
    }

    virtual std::shared_ptr<Object> __getitem__(std::shared_ptr<Object> idx) {
        throw RuntimeError("object is not subscriptable");
    }

    virtual bool __contains__(std::shared_ptr<Object> item) {
        throw RuntimeError("object is not iterable");
    }
    
    virtual std::shared_ptr<Object> getattr(const std::string &name) {
        throw RuntimeError("object has no attribute '" + name + "'");
    }

    virtual ObjectPtr __call__(const std::vector<ObjectPtr> & args) {
        throw RuntimeError("object is not callable");
    }

    virtual std::type_index type() const = 0;
    virtual std::string repr() const = 0;
};

class PyNone : public Object {
public:
    PyNone() = default;

    std::type_index type() const override {
        return typeid(PyNone);
    }

    std::string repr() const override {
        return "None";
    }
};

class PyInt : public Object {
    int value;
public:
    PyInt(int v) : value(v) {}

    ObjectPtr __add__(ObjectPtr right) override {
        if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
            return std::make_shared<PyInt>(value + r->value);
        }
        throw RuntimeError("unsupported operand types for +: 'int' and '" + std::string(right->repr()) + "'");
    }
    
    ObjectPtr __sub__(ObjectPtr right) override {
        if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
            return std::make_shared<PyInt>(value - r->value);
        }
        throw RuntimeError("unsupported operand types for -: 'int' and '" + std::string(right->repr()) + "'");
    }

    std::type_index type()  const override { 
        return typeid(PyInt); 
    }

    std::string repr() const override { 
        return std::to_string(value); 
    }

    int get() const { 
        return value; 
    }
};

class PyBool : public Object {
    bool value;
public:
    PyBool(bool v): value(v) {}

    // True -> 1, False -> 0, возвращаем PyInt
    ObjectPtr __add__(ObjectPtr right) override {
        int a = value ? 1 : 0;

        if (auto r_bool = std::dynamic_pointer_cast<PyBool>(right)) {
            int b = r_bool->value ? 1 : 0;
            return std::make_shared<PyInt>(a + b);
        }

        if (auto r_int = std::dynamic_pointer_cast<PyInt>(right)) {
            return std::make_shared<PyInt>(a + r_int->get());
        }

        throw RuntimeError(
            "unsupported operand types for +: 'bool' and '" + right->repr() + "'"
        );
    }

    ObjectPtr __sub__(ObjectPtr right) override {
        int a = value ? 1 : 0;

        if (auto r_bool = std::dynamic_pointer_cast<PyBool>(right)) {
            int b = r_bool->value ? 1 : 0;
            return std::make_shared<PyInt>(a - b);
        }

        if (auto r_int = std::dynamic_pointer_cast<PyInt>(right)) {
            return std::make_shared<PyInt>(a - r_int->get());
        }
        
        throw RuntimeError(
            "unsupported operand types for -: 'bool' and '" + right->repr() + "'"
        );
    }

    std::type_index type()  const override { 
        return typeid(PyBool); 
    }

    std::string repr() const override {
        return value ? "True" : "False";
    }

    bool get() const { 
        return value; 
    }
};

class PyFloat : public Object {
    double value;
public:
    PyFloat(double v) : value(v) {}

    ObjectPtr __add__(ObjectPtr right) override {
        if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
            return std::make_shared<PyFloat>(value + r->value);
        }

        throw RuntimeError("unsupported operand types for +: 'float' and '" + right->repr() + "'");
    }

    ObjectPtr __sub__(ObjectPtr right) override {
        if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
            return std::make_shared<PyFloat>(value - r->value);
        }
    
        throw RuntimeError("unsupported operand types for -: 'float' and '" + right->repr() + "'");
    }

    std::type_index type()  const override { 
        return typeid(PyFloat); 
    }

    std::string repr() const override {
        std::ostringstream ss;
        ss << value;
        return ss.str();
    }

    double get() const { 
        return value; 
    }
};


class PyString : public Object {
    std::string value;
public:
    PyString(std::string v) : value(std::move(v)) {}

    ObjectPtr __add__(ObjectPtr right) override {
        if (auto r = std::dynamic_pointer_cast<PyString>(right)) {
            return std::make_shared<PyString>(value + r->value);
        }
        
        throw RuntimeError("unsupported operand types for +: 'str' and '" + right->repr() + "'");
    }

    ObjectPtr __getitem__(ObjectPtr idx) override {
        // Только целый индекс
        auto iobj = std::dynamic_pointer_cast<PyInt>(idx);
        
        if (!iobj) {
            throw RuntimeError("string indices must be integers");
        }

        int i = iobj->get();
        if (i < 0 || i >= int(value.size())) {
            throw RuntimeError("string index out of range");
        }

        return std::make_shared<PyString>(std::string(1, value[i]));
    }

    bool __contains__(ObjectPtr item) override {
        auto sub = std::dynamic_pointer_cast<PyString>(item);

        if (!sub) {
            throw RuntimeError("'in' requires a string as right operand");
        }

        return value.find(sub->value) != std::string::npos;
    }

    std::type_index type()  const override { 
        return typeid(PyString); 
    }

    std::string repr() const override { 
        return '"' + value + '"'; 
    }

    const std::string &get() const { 
        return value; 
    }
};


class PyList : public Object {
    std::vector<ObjectPtr> elems;
public:
    PyList(std::vector<ObjectPtr> v) : elems(std::move(v)) {}

    ObjectPtr __add__(ObjectPtr right) override {
        auto r = std::dynamic_pointer_cast<PyList>(right);

        if (!r) {
            throw RuntimeError("can only concatenate list (not '" + right->repr() + "') to list");
        }

        std::vector<ObjectPtr> merged = elems;
        merged.insert(merged.end(), r->elems.begin(), r->elems.end());
        return std::make_shared<PyList>(std::move(merged));
    }

    ObjectPtr __getitem__(ObjectPtr idx) override {
        auto iobj = std::dynamic_pointer_cast<PyInt>(idx);

        if (!iobj) {
            throw RuntimeError("list indices must be integers");
        }

        int i = iobj->get();
        if (i < 0 || i >= int(elems.size()))
            throw RuntimeError("list index out of range");
        return elems[i];
    }

    bool __contains__(ObjectPtr item) override {
        for (auto &el : elems) {
            if (el->repr() == item->repr()) return true;
        }
        return false;
    }

    std::type_index type()  const override { return typeid(PyList); }
    std::string      repr() const override {
        std::string s = "[";
        bool first = true;
        for (auto &el : elems) {
            if (!first) s += ", ";
            s += el->repr();
            first = false;
        }
        return s + "]";
    }
};

class PyDict : public Object {
    // храним (key,value), поиск по key->repr()
    std::vector<std::pair<ObjectPtr,ObjectPtr>> items;
public:
    PyDict() = default;

    // задать или перезаписать
    void setItem(ObjectPtr key, ObjectPtr val) {
        for (auto &p : items) {
            if (p.first->repr() == key->repr()) {
                p.second = val;
                return;
            }
        }
        items.emplace_back(key,val);
    }

    ObjectPtr __getitem__(ObjectPtr idx) override {
        for (auto &p : items) {
            if (p.first->repr() == idx->repr())
                return p.second;
        }
        throw RuntimeError("KeyError: " + idx->repr());
    }
    bool __contains__(ObjectPtr idx) override {
        for (auto &p : items) {
            if (p.first->repr() == idx->repr())
                return true;
        }
        return false;
    }

    std::type_index type()  const override { return typeid(PyDict); }
    std::string      repr() const override {
        std::string s = "{"; bool first = true;
        for (auto &p : items) {
            if (!first) s += ", ";
            s += p.first->repr() + ": " + p.second->repr();
            first = false;
        }
        return s + "}";
    }
};


class PySet : public Object {
    std::vector<ObjectPtr> elems;
public:
    PySet() = default;
    PySet(std::vector<ObjectPtr> v) {
        for (auto &e : v) {
            if (!__contains__(e))
                elems.push_back(e);
        }
    }

    // union: a | b
    ObjectPtr __add__(ObjectPtr right) override {
        auto r = std::dynamic_pointer_cast<PySet>(right);

        if (!r) {
            throw RuntimeError("unsupported operand for set union");
        }
        
        auto result = std::make_shared<PySet>(elems);
        for (auto &e : r->elems) {
            if (!result->__contains__(e))
                result->elems.push_back(e);
        }
        return result;
    }

    bool __contains__(ObjectPtr item) override {
        for (auto &e : elems) {
            if (e->repr() == item->repr())
                return true;
        }
        return false;
    }

    std::type_index type()  const override { return typeid(PySet); }
    std::string      repr() const override {
        std::string s = "{"; bool first = true;
        for (auto &e : elems) {
            if (!first) s += ", ";
            s += e->repr();
            first = false;
        }
        return s + "}";
    }
};

class PyBuiltinFunction : public Object {
    std::string name;
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn;
public:
    // Конструктор принимает имя (для repr) и саму функцию-имплементацию
    PyBuiltinFunction(std::string name, std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn) : 
    name(std::move(name)), fn(std::move(fn)) {}

    // std::function
    ObjectPtr __call__(const std::vector<ObjectPtr> &args) override {
        return fn(args);
    }

    std::type_index type()  const override {
        return typeid(PyBuiltinFunction);
    }

    std::string repr() const override {
        return "<built-in function " + name + ">";
    }
};

class PyFunction : public Object {
    
    FuncDecl *decl;
    std::shared_ptr<SymbolTable> scope;

public:
    PyFunction(FuncDecl *f, std::shared_ptr<SymbolTable> scope) : decl(f), scope(std::move(scope)) {}

    ObjectPtr __call__(const std::vector<ObjectPtr> &args) override {
        
        auto localTable = std::make_shared<SymbolTable>(scope);

        // 1. pos_params
        const auto &names = decl->posParams;
        if (args.size() < names.size()) {
            throw RuntimeError(decl->name + 
                "() missing required positional arguments");
        }
        for (size_t i = 0; i < names.size(); ++i) {
            Symbol s;
            s.name  = names[i];
            s.type  = SymbolType::Parameter;
            s.value = args[i];
            localTable->insert(s);
        }

        // 2. def_params (todo)
        for (const auto &pd : decl->defaultParams) {
            const std::string &paramName = pd.first;
            
            if (localTable->lookup_local(paramName) == nullptr) {
                
                Symbol s;
                s.name  = paramName;
                s.type  = SymbolType::Parameter;
                s.value = std::make_shared<PyNone>();
                localTable->insert(s);
            }
        }

        // body (todo)

        throw RuntimeError("function execution is not implemented yet");

    }

    std::type_index type() const override {
        return typeid(PyFunction);
    }

    std::string repr() const override {
        
        std::ostringstream ss;
        ss << "<function " << decl->name << " at " << this << ">";
        return ss.str();
    }

    
    FuncDecl* getDecl() const {
        return decl;
    }
};