#pragma once

#include "ast.hpp"        // для FuncDecl и т.п., если нужно
#include "symbol_table.hpp"
#include "executer_excepts.hpp"
#include <functional>
#include <sstream>
#include <memory>
#include <vector>
#include <typeindex>
#include <string>
#include <stdexcept>    

// -----------------------------------------------------------------------------
// Форвардные объявления: говорим компилятору, что существую эти классы,
// но пока без описания. Это необходимо, чтобы внутри Object могли быть указатели
// на эти типы, а тела методов самих классов мы перенесём вниз.
// -----------------------------------------------------------------------------


class PyBool;
class PyFloat;
class PyString;
class PyList;
class PyDict;
class PySet;
class PyBuiltinFunction;
class PyFunction;
class PyNone;
class PyInstance;
class PyClass;

// -----------------------------------------------------------------------------
// Object и RuntimeError
// -----------------------------------------------------------------------------
class Object;
using ObjectPtr = std::shared_ptr<Object>;

struct RuntimeError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Object {
public:
    virtual ~Object() = default;

    virtual ObjectPtr __add__(ObjectPtr right) {
        throw RuntimeError("unsupported operand types for +");
    }
    virtual ObjectPtr __sub__(ObjectPtr right) {
        throw RuntimeError("unsupported operand types for -");
    }
    virtual ObjectPtr __mul__(ObjectPtr right) {
        throw RuntimeError("unsupported operand types for *");
    }
    virtual ObjectPtr __div__(ObjectPtr right) {
        throw RuntimeError("unsupported operand types for /");
    }

    virtual ObjectPtr __getitem__(ObjectPtr idx) {
        throw RuntimeError("object is not subscriptable");
    }
    virtual void __setitem__(ObjectPtr key, ObjectPtr value) {
        throw RuntimeError("object does not support item assignment");
    }

    virtual void __setattr__(const std::string &name, ObjectPtr value) {
        throw RuntimeError("object has no attribute '" + name + "'");
    }
    virtual bool __contains__(ObjectPtr item) {
        throw RuntimeError("object is not iterable");
    }
    virtual ObjectPtr __getattr__(const std::string &name) {
        throw RuntimeError("object has no attribute '" + name + "'");
    }

    virtual ObjectPtr __call__(const std::vector<ObjectPtr> & args) {
        throw RuntimeError("object is not callable");
    }

    virtual std::type_index type() const = 0;
    virtual std::string repr() const = 0;
};

// -----------------------------------------------------------------------------
// PyNone
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// PyInt: объявление (тела методов будут определены ниже, после всех классов).
// -----------------------------------------------------------------------------
class PyInt : public Object {
    int value;
public:
    PyInt(int v);
    // Методы-операторы (__add__, __sub__ и т.д.) мы реализуем ниже,
    // после полной декларации PyBool, PyFloat, PyString, PyList и т.д.
    ObjectPtr __add__(ObjectPtr right) override;
    ObjectPtr __sub__(ObjectPtr right) override;
    ObjectPtr __mul__(ObjectPtr right) override;
    ObjectPtr __div__(ObjectPtr right) override;

    std::type_index type() const override;
    std::string repr() const override;

    int get() const;
};

// -----------------------------------------------------------------------------
// PyBool: объявление (тела методов – ниже).
// -----------------------------------------------------------------------------
class PyBool : public Object {
    bool value;
public:
    PyBool(bool v);
    ObjectPtr __add__(ObjectPtr right) override;
    ObjectPtr __sub__(ObjectPtr right) override;
    ObjectPtr __mul__(ObjectPtr right) override;
    ObjectPtr __div__(ObjectPtr right) override;

    std::type_index type() const override;
    std::string repr() const override;

    bool get() const;
};

// -----------------------------------------------------------------------------
// PyFloat: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyFloat : public Object {
    double value;
public:
    PyFloat(double v);
    ObjectPtr __add__(ObjectPtr right) override;
    ObjectPtr __sub__(ObjectPtr right) override;
    ObjectPtr __mul__(ObjectPtr right) override;
    ObjectPtr __div__(ObjectPtr right) override;

    std::type_index type() const override;
    std::string repr() const override;

    double get() const;
};

// -----------------------------------------------------------------------------
// PyString: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyString : public Object {
    std::string value;
public:
    PyString(std::string v);
    ObjectPtr __add__(ObjectPtr right) override;
    ObjectPtr __mul__(ObjectPtr right) override;
    ObjectPtr __getitem__(ObjectPtr idx) override;
    bool __contains__(ObjectPtr item) override;

    std::type_index type() const override;
    std::string repr() const override;

    const std::string &get() const;
};

// -----------------------------------------------------------------------------
// PyList: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyList : public Object {
    std::vector<ObjectPtr> elems;
public:
    PyList(std::vector<ObjectPtr> v);
    ObjectPtr __add__(ObjectPtr right) override;
    ObjectPtr __mul__(ObjectPtr right) override;
    ObjectPtr __getitem__(ObjectPtr idx) override;
    void __setitem__(ObjectPtr idx, ObjectPtr value) override;
    bool __contains__(ObjectPtr item) override;

    std::type_index type() const override;
    std::string repr() const override;

    // Нам пригодится метод получения вектора элементов:
    const std::vector<ObjectPtr>& getElements() const;
};

// -----------------------------------------------------------------------------
// PyDict: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyDict : public Object {
    // храним (key,value), поиск по key->repr()
    std::vector<std::pair<ObjectPtr,ObjectPtr>> items;
public:
    PyDict();
    void __setitem__(ObjectPtr key, ObjectPtr val);
    ObjectPtr __getitem__(ObjectPtr idx) override;
    bool __contains__(ObjectPtr idx) override;

    std::type_index type() const override;
    std::string repr() const override;

    const std::vector<std::pair<ObjectPtr,ObjectPtr>>& getItems() const;
};

// -----------------------------------------------------------------------------
// PySet: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PySet : public Object {
    std::vector<ObjectPtr> elems;
public:
    PySet();
    PySet(std::vector<ObjectPtr> v);
    ObjectPtr __add__(ObjectPtr right) override; // union ( | )
    bool __contains__(ObjectPtr item) override;

    std::type_index type() const override;
    std::string repr() const override;

    const std::vector<ObjectPtr>& getElements() const;
};

// -----------------------------------------------------------------------------
// PyBuiltinFunction: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyBuiltinFunction : public Object {
    std::string name;
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn;
public:
    PyBuiltinFunction(std::string name, std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn);
    ObjectPtr __call__(const std::vector<ObjectPtr> &args) override;

    std::type_index type() const override;
    std::string repr() const override;
};

// -----------------------------------------------------------------------------
// PyFunction: объявление (реализацию – ниже).
// -----------------------------------------------------------------------------
class PyFunction : public Object {
    // Имя функции (чтобы было понятно, как она называется при repr())
    std::string name;
    // AST-узел, в котором описано тело (FuncDecl* позволяет нам найти posParams, defaultParams и сам код)
    FuncDecl *decl;
    // Лексическое окружение (таблица символов), где функция была объявлена, 
    // чтобы обеспечить замыкания (клаузуру)
    std::shared_ptr<SymbolTable> scope;
    // Список имён позиционных (обязательных) параметров
    std::vector<std::string> posParams;
    // Вектор заранее вычисленных default-значений (порядок тот же, что и в decl->defaultParams)
    std::vector<ObjectPtr> defaultValues;

public:
    PyFunction(const std::string &funcName,
               FuncDecl *f,
               std::shared_ptr<SymbolTable> enclosingEnv,
               const std::vector<std::string> &parameters,
               std::vector<ObjectPtr> defaults);

    
    const std::vector<std::string>& getPosParams() const {
        return posParams;
    }
   
    const std::vector<ObjectPtr>& getDefaultValues() const {
        return defaultValues;
    }

    ObjectPtr __call__(const std::vector<ObjectPtr> &args) override;

    std::type_index type() const override;
    std::string repr() const override;

    FuncDecl* getDecl() const;
};

// -----------------------------------------------------------------------------
// PyInstance и PyClass (у них я оставил всё как есть, реализации внутри класса – в порядке).
// -----------------------------------------------------------------------------
class PyInstance : public Object {
public:
    std::shared_ptr<PyClass> classPtr;
    std::shared_ptr<PyDict> instanceDict;

    PyInstance(std::shared_ptr<PyClass> cls);

    // Сначала смотрим в instanceDict, потом – в classPtr->classDict
    ObjectPtr __getattr__(const std::string &name) override;
    void __setattr__(const std::string &name, ObjectPtr value) override;
    ObjectPtr __call__(const std::vector<ObjectPtr> & /*args*/) override;

    std::string repr() const override;
    std::type_index type() const override;
};

class PyClass : public Object, public std::enable_shared_from_this<PyClass> {
public:
    // Имя класса
    std::string name;
    // Словарь атрибутов/методов данного класса
    std::shared_ptr<PyDict> classDict;

    // Указатель на родительский класс (nullptr, если класс не наследуется)
    std::shared_ptr<PyClass> parent;

    // Конструктор: теперь принимает имя класса и (опционально) родителя
    PyClass(std::string className, std::shared_ptr<PyClass> baseClass = nullptr);

    // При запросе атрибута: сначала ищем в classDict, 
    // если не нашли и parent != nullptr — идём к parent
    ObjectPtr __getattr__(const std::string &attrName) override;

    // Запись атрибута в classDict без помех; parent остается неизменным
    void __setattr__(const std::string &attrName, ObjectPtr value) override;

    // Вызов конструктора класса: создаём экземпляр, затем вызываем __init__, 
    // причём __init__ может быть унаследован из parent, если не переопределён.
    ObjectPtr __call__(const std::vector<ObjectPtr> &args) override;

    std::string repr() const override;
    std::type_index type() const override;
    
    std::shared_ptr<PyDict> getClassDict() const {
        return classDict;
    }
};

// -----------------------------------------------------------------------------
// Теперь, после того, как мы ДЕКЛАРИРОВАЛИ все нужные классы (PyInt, PyBool, PyFloat, PyString, PyList и т.д.),
// можно дать полные тела всех методов, которые ранее ссылались на методы других классов.
// -----------------------------------------------------------------------------

// -------------------- PyInt --------------------
inline PyInt::PyInt(int v) : value(v) {}

inline ObjectPtr PyInt::__add__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(value + r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        // r->get() теперь допустим, потому что PyBool уже полностью определён
        return std::make_shared<PyInt>(value + (r->get() ? 1 : 0));
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        // r->get() допустим, потому что PyFloat уже определён
        return std::make_shared<PyFloat>(value + r->get());
    }
    throw RuntimeError("unsupported operand types for +: 'int' and '" + right->repr() + "'");
}

inline ObjectPtr PyInt::__sub__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(value - r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        return std::make_shared<PyInt>(value - (r->get() ? 1 : 0));
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(value - r->get());
    }
    throw RuntimeError("unsupported operand types for -: 'int' and '" + right->repr() + "'");
}

inline ObjectPtr PyInt::__mul__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(value * r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        return std::make_shared<PyInt>(value * (r->get() ? 1 : 0));
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(value * r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyString>(right)) {
        // int * str → повторение строки
        int times = value;
        if (times < 0) times = 0;
        std::string base = r->get();  // r->get() допустим
        std::string accum;
        for (int i = 0; i < times; ++i) {
            accum += base;
        }
        return std::make_shared<PyString>(accum);
    }
    if (auto r = std::dynamic_pointer_cast<PyList>(right)) {
        // int * list → повторение списка
        int times = value;
        if (times < 0) times = 0;
        const auto &base = r->getElements();  // getElements() допустим
        std::vector<ObjectPtr> accum;
        for (int i = 0; i < times; ++i) {
            for (auto &el : base) {
                accum.push_back(el);
            }
        }
        return std::make_shared<PyList>(std::move(accum));
    }
    throw RuntimeError("unsupported operand types for *: 'int' and '" + right->repr() + "'");
}

inline ObjectPtr PyInt::__div__(ObjectPtr right) {
    double a = value;
    double b;
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        b = r->value;
    } else if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        b = r->get() ? 1.0 : 0.0;
    } else if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        b = r->get();
    } else {
        throw RuntimeError("unsupported operand types for /: 'int' and '" + right->repr() + "'");
    }
    if (b == 0.0) {
        throw RuntimeError("ZeroDivisionError: division by zero");
    }
    return std::make_shared<PyFloat>(a / b);
}

inline std::type_index PyInt::type() const {
    return typeid(PyInt);
}

inline std::string PyInt::repr() const {
    return std::to_string(value);
}

inline int PyInt::get() const {
    return value;
}

// -------------------- PyBool --------------------
inline PyBool::PyBool(bool v) : value(v) {}

inline ObjectPtr PyBool::__add__(ObjectPtr right) {
    int a = value ? 1 : 0;
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        int b = r->get();
        return std::make_shared<PyInt>(a + b);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(a + r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(a + r->get());
    }
    throw RuntimeError("unsupported operand types for +: 'bool' and '" + right->repr() + "'");
}

inline ObjectPtr PyBool::__sub__(ObjectPtr right) {
    int a = value ? 1 : 0;
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        int b = r->get();
        return std::make_shared<PyInt>(a - b);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(a - r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(a - r->get());
    }
    throw RuntimeError("unsupported operand types for -: 'bool' and '" + right->repr() + "'");
}

inline ObjectPtr PyBool::__mul__(ObjectPtr right) {
    int a = value ? 1 : 0;
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        int b = r->get();
        return std::make_shared<PyInt>(a * b);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyInt>(a * r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(a * r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyString>(right)) {
        // bool * str → конвертируем bool в int
        int times = a;
        if (times < 0) times = 0;
        std::string base = r->get();
        std::string accum;
        for (int i = 0; i < times; ++i) {
            accum += base;
        }
        return std::make_shared<PyString>(accum);
    }
    if (auto r = std::dynamic_pointer_cast<PyList>(right)) {
        int times = a;
        if (times < 0) times = 0;
        const auto &base = r->getElements();
        std::vector<ObjectPtr> accum;
        for (int i = 0; i < times; ++i) {
            for (auto &el : base) {
                accum.push_back(el);
            }
        }
        return std::make_shared<PyList>(std::move(accum));
    }
    throw RuntimeError("unsupported operand types for *: 'bool' and '" + right->repr() + "'");
}

inline ObjectPtr PyBool::__div__(ObjectPtr right) {
    double a = value ? 1.0 : 0.0;
    double b;
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        b = r->get() ? 1.0 : 0.0;
    } else if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        b = r->get();
    } else if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        b = r->get();
    } else {
        throw RuntimeError("unsupported operand types for /: 'bool' and '" + right->repr() + "'");
    }
    if (b == 0.0) {
        throw RuntimeError("ZeroDivisionError: division by zero");
    }
    return std::make_shared<PyFloat>(a / b);
}

inline std::type_index PyBool::type() const {
    return typeid(PyBool);
}

inline std::string PyBool::repr() const {
    return value ? "True" : "False";
}

inline bool PyBool::get() const {
    return value;
}

// -------------------- PyFloat --------------------
inline PyFloat::PyFloat(double v) : value(v) {}

inline ObjectPtr PyFloat::__add__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(value + r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyFloat>(value + r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        return std::make_shared<PyFloat>(value + (r->get() ? 1.0 : 0.0));
    }
    throw RuntimeError("unsupported operand types for +: 'float' and '" + right->repr() + "'");
}

inline ObjectPtr PyFloat::__sub__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(value - r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyFloat>(value - r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        return std::make_shared<PyFloat>(value - (r->get() ? 1.0 : 0.0));
    }
    throw RuntimeError("unsupported operand types for -: 'float' and '" + right->repr() + "'");
}

inline ObjectPtr PyFloat::__mul__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        return std::make_shared<PyFloat>(value * r->value);
    }
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        return std::make_shared<PyFloat>(value * r->get());
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        return std::make_shared<PyFloat>(value * (r->get() ? 1.0 : 0.0));
    }
    throw RuntimeError("unsupported operand types for *: 'float' and '" + right->repr() + "'");
}

inline ObjectPtr PyFloat::__div__(ObjectPtr right) {
    double b;
    if (auto r = std::dynamic_pointer_cast<PyFloat>(right)) {
        b = r->get();
    } else if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        b = r->get();
    } else if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        b = r->get() ? 1.0 : 0.0;
    } else {
        throw RuntimeError("unsupported operand types for /: 'float' and '" + right->repr() + "'");
    }
    if (b == 0.0) {
        throw RuntimeError("ZeroDivisionError: division by zero");
    }
    return std::make_shared<PyFloat>(value / b);
}

inline std::type_index PyFloat::type() const {
    return typeid(PyFloat);
}

inline std::string PyFloat::repr() const {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

inline double PyFloat::get() const {
    return value;
}

// -------------------- PyString --------------------
inline PyString::PyString(std::string v) : value(std::move(v)) {}

inline ObjectPtr PyString::__add__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyString>(right)) {
        return std::make_shared<PyString>(value + r->value);
    }
    throw RuntimeError("unsupported operand types for +: 'str' and '" + right->repr() + "'");
}

inline ObjectPtr PyString::__mul__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        int times = r->get();
        if (times < 0) times = 0;
        std::string accum;
        for (int i = 0; i < times; ++i) {
            accum += value;
        }
        return std::make_shared<PyString>(accum);
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        int times = r->get() ? 1 : 0;
        std::string accum;
        for (int i = 0; i < times; ++i) {
            accum += value;
        }
        return std::make_shared<PyString>(accum);
    }
    throw RuntimeError("unsupported operand types for *: 'str' and '" + right->repr() + "'");
}

inline ObjectPtr PyString::__getitem__(ObjectPtr idx) {
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

inline bool PyString::__contains__(ObjectPtr item) {
    auto sub = std::dynamic_pointer_cast<PyString>(item);
    if (!sub) {
        throw RuntimeError("'in' requires a string as right operand");
    }
    return value.find(sub->value) != std::string::npos;
}

inline std::type_index PyString::type() const {
    return typeid(PyString);
}

inline std::string PyString::repr() const {
    return '"' + value + '"';
}

inline const std::string &PyString::get() const {
    return value;
}

// -------------------- PyList --------------------
inline PyList::PyList(std::vector<ObjectPtr> v) : elems(std::move(v)) {}

inline ObjectPtr PyList::__add__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyList>(right)) {
        std::vector<ObjectPtr> merged = elems;
        merged.insert(merged.end(), r->elems.begin(), r->elems.end());
        return std::make_shared<PyList>(std::move(merged));
    }
    throw RuntimeError("can only concatenate list (not '" + right->repr() + "') to list");
}

inline ObjectPtr PyList::__mul__(ObjectPtr right) {
    if (auto r = std::dynamic_pointer_cast<PyInt>(right)) {
        int times = r->get();
        if (times < 0) times = 0;
        std::vector<ObjectPtr> accum;
        for (int i = 0; i < times; ++i) {
            for (auto &el : elems) {
                accum.push_back(el);
            }
        }
        return std::make_shared<PyList>(std::move(accum));
    }
    if (auto r = std::dynamic_pointer_cast<PyBool>(right)) {
        int times = r->get() ? 1 : 0;
        std::vector<ObjectPtr> accum;
        for (int i = 0; i < times; ++i) {
            for (auto &el : elems) {
                accum.push_back(el);
            }
        }
        return std::make_shared<PyList>(std::move(accum));
    }
    throw RuntimeError("unsupported operand types for *: 'list' and '" + right->repr() + "'");
}

inline ObjectPtr PyList::__getitem__(ObjectPtr idx) {
    auto iobj = std::dynamic_pointer_cast<PyInt>(idx);
    if (!iobj) {
        throw RuntimeError("list indices must be integers");
    }
    int i = iobj->get();
    if (i < 0 || i >= int(elems.size())) {
        throw RuntimeError("list index out of range");
    }
    return elems[i];
}

inline void PyList::__setitem__(ObjectPtr idx, ObjectPtr value) {
    auto iobj = std::dynamic_pointer_cast<PyInt>(idx);
    if (!iobj) {
        throw RuntimeError("list indices must be integers");
    }
    int i = iobj->get();
    if (i < 0 || i >= static_cast<int>(elems.size())) {
        throw RuntimeError("list index out of range");
    }
    elems[i] = value;
}

inline bool PyList::__contains__(ObjectPtr item) {
    for (auto &el : elems) {
        if (el->repr() == item->repr()) return true;
    }
    return false;
}

inline std::type_index PyList::type() const {
    return typeid(PyList);
}

inline std::string PyList::repr() const {
    std::string s = "[";
    bool first = true;
    for (auto &el : elems) {
        if (!first) s += ", ";
        s += el->repr();
        first = false;
    }
    return s + "]";
}

inline const std::vector<ObjectPtr>& PyList::getElements() const {
    return elems;
}

// -------------------- PyDict --------------------
inline PyDict::PyDict() = default;

inline void PyDict::__setitem__(ObjectPtr key, ObjectPtr val) {
    for (auto &p : items) {
        if (p.first->repr() == key->repr()) {
            p.second = val;
            return;
        }
    }
    items.emplace_back(key, val);
}

inline ObjectPtr PyDict::__getitem__(ObjectPtr idx) {
    for (auto &p : items) {
        if (p.first->repr() == idx->repr())
            return p.second;
    }
    throw RuntimeError("KeyError: " + idx->repr());
}

inline bool PyDict::__contains__(ObjectPtr idx) {
    for (auto &p : items) {
        if (p.first->repr() == idx->repr())
            return true;
    }
    return false;
}

inline std::type_index PyDict::type() const {
    return typeid(PyDict);
}

inline std::string PyDict::repr() const {
    std::string s = "{";
    bool first = true;
    for (auto &p : items) {
        if (!first) s += ", ";
        s += p.first->repr() + ": " + p.second->repr();
        first = false;
    }
    return s + "}";
}

inline const std::vector<std::pair<ObjectPtr,ObjectPtr>>& PyDict::getItems() const {
    return items;
}

// -------------------- PySet --------------------
inline PySet::PySet() = default;

inline PySet::PySet(std::vector<ObjectPtr> v) {
    for (auto &e : v) {
        if (!e) {
        throw RuntimeError("internal error: null element in set constructor");
        }
        if (!__contains__(e))
            elems.push_back(e);
    }
}

inline ObjectPtr PySet::__add__(ObjectPtr right) {
    auto r = std::dynamic_pointer_cast<PySet>(right);
    if (!r) {
        throw RuntimeError("unsupported operand for set union");
    }
    auto result = std::make_shared<PySet>(elems);
    for (auto &e : r->elems) {
        if (!result->__contains__(e)) {
            result->elems.push_back(e);
        }
    }
    return result;
}

inline bool PySet::__contains__(ObjectPtr item) {
    for (auto &e : elems) {
        if (e->repr() == item->repr())
            return true;
    }
    return false;
}

inline std::type_index PySet::type() const {
    return typeid(PySet);
}

inline std::string PySet::repr() const {
    std::string s = "{";
    bool first = true;
    for (auto &e : elems) {
        if (!first) s += ", ";
        s += e->repr();
        first = false;
    }
    return s + "}";
}

inline const std::vector<ObjectPtr>& PySet::getElements() const {
    return elems;
}

// -------------------- PyBuiltinFunction --------------------
inline PyBuiltinFunction::PyBuiltinFunction(
    std::string name,
    std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn
) : name(std::move(name)), fn(std::move(fn)) {}

inline ObjectPtr PyBuiltinFunction::__call__(const std::vector<ObjectPtr> &args) {
    return fn(args);
}

inline std::type_index PyBuiltinFunction::type() const {
    return typeid(PyBuiltinFunction);
}

inline std::string PyBuiltinFunction::repr() const {
    return "<built-in function " + name + ">";
}


// -------------------- PyInstance --------------------
inline PyInstance::PyInstance(std::shared_ptr<PyClass> cls)
    : classPtr(std::move(cls)), instanceDict(std::make_shared<PyDict>())
{}

inline ObjectPtr PyInstance::__getattr__(const std::string &name) {
    auto key = std::make_shared<PyString>(name);
    if (instanceDict->__contains__(key)) {
        return instanceDict->__getitem__(key);
    }
    return classPtr->__getattr__(name);
}

inline void PyInstance::__setattr__(const std::string &name, ObjectPtr value) {
    auto key = std::make_shared<PyString>(name);
    instanceDict->__setitem__(key, value);
}

inline ObjectPtr PyInstance::__call__(const std::vector<ObjectPtr> & /*args*/) {
    throw RuntimeError("object is not callable");
}

inline std::string PyInstance::repr() const {
    std::ostringstream ss;
    ss << "<Instance of " << classPtr->name << " at " << this << ">";
    return ss.str();
}

inline std::type_index PyInstance::type() const {
    return typeid(PyInstance);
}

// -------------------- PyClass --------------------
inline PyClass::PyClass(std::string className, std::shared_ptr<PyClass> baseClass)
    : name(className),
      classDict(std::make_shared<PyDict>()),
      parent(std::move(baseClass))
{}

// Поиск атрибута у класса: 
//   1) Сначала проверяем текущее classDict
//   2) Если не нашли и parent != nullptr, рекурсивно пытаемся найти у parent
inline ObjectPtr PyClass::__getattr__(const std::string &attrName) {
    auto key = std::make_shared<PyString>(attrName);
    if (classDict->__contains__(key)) {
        return classDict->__getitem__(key);
    }
    if (parent) {
        // Если у родителя есть такой атрибут — возвращаем его
        return parent->__getattr__(attrName);
    }
    throw RuntimeError("object has no attribute '" + attrName + "'");
}

inline void PyClass::__setattr__(const std::string &attrName, ObjectPtr value) {
    auto key = std::make_shared<PyString>(attrName);
    classDict->__setitem__(key, value);
}

// Вызов «конструктора» класса:
//   1) Создаём новый PyInstance(this), 
//   2) Вычисляем, есть ли __init__ в classDict или унаследован из parent,
//   3) Если есть, вызываем его (первым аргументом self), ловим ReturnException, если он есть,
//      но не возвращаем его, потому что __init__ в Python практически всегда возвращает None.
//   4) Возвращаем созданный экземпляр.
inline ObjectPtr PyClass::__call__(const std::vector<ObjectPtr> &args) {
    // 1) Новый экземпляр
    auto instance = std::make_shared<PyInstance>(shared_from_this());

        // 2) Готовим искать метод "__init__" в цепочке классов
    auto initKey = std::make_shared<PyString>("__init__");
    try {
            // Пытаемся найти "__init__" через __getattr__ (поднимется по родителям, если нужно)
        std::shared_ptr<void> initObjRaw = __getattr__("__init__");
            // Приводим к PyFunction
        auto initFn = std::dynamic_pointer_cast<PyFunction>(
            std::static_pointer_cast<Object>(initObjRaw)
        );
        if (!initFn) {
            // Если нашли что-то, но это не PyFunction → TypeError
            throw RuntimeError("__init__ is not callable");
        }

        // 2.1) Собираем аргументы: первым аргументом передаём self, а затем — остальные
        std::vector<std::shared_ptr<Object>> initArgs;
        initArgs.reserve(1 + args.size());
        initArgs.push_back(instance);
        for (auto &a : args) {
            initArgs.push_back(a);
        }

        // 2.2) Вызываем __init__. Если внутри упадёт ReturnException — это просто вернёт управление сюда
        try {
            initFn->__call__(initArgs);
        }
        catch (const ReturnException &) {
            // «return» из __init__ игнорируем — в Python __init__ не должен
            // ничего возвращать, просто инициализировать
        }
    }
        catch (const RuntimeError &err) {
            // Если __getattr__("__init__") выкинул RuntimeError с сообщением
            // "object has no attribute '__init__'" или другим, значит __init__ не найден —
            // просто пропускаем и возвращаем «пустой» экземпляр без инициализации.
        }

    return instance;
}

inline std::string PyClass::repr() const {
    std::ostringstream ss;
    ss << "<class " << name << " at " << this << ">";
    return ss.str();
}

inline std::type_index PyClass::type() const {
    return typeid(PyClass);
}
