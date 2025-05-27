#pragma once

#include <typeindex>
#include <vector>
#include <memory>

class Object;
using ObjectPtr = std::shared_ptr<Object>;

struct RuntimeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Object {
public:
    virtual ~Object() = default;

    virtual std::shared_ptr<Object> __add__(std::shared_ptr<Object> rhs) {
        throw RuntimeError("unsupported operand types for +");
    }

    virtual std::shared_ptr<Object> __sub__(std::shared_ptr<Object> rhs) {
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

    virtual std::type_index type() const = 0;
    virtual std::string repr() const = 0;
};