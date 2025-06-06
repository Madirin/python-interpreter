#include "executer.hpp"
#include "type_registry.hpp"
#include "object.hpp"
#include "executer_excepts.hpp"
#include <unordered_set>
#include <memory>
#include <iostream>



auto is_truthy(ObjectPtr &obj) {
        if (auto b = std::dynamic_pointer_cast<PyBool>(obj)) {
            return b->get();
        }
        if (auto i = std::dynamic_pointer_cast<PyInt>(obj)) {
            return i->get() != 0;
        }
        if (auto f = std::dynamic_pointer_cast<PyFloat>(obj)) {
            return f->get() != 0.0;
        }
        if (auto s = std::dynamic_pointer_cast<PyString>(obj)) {
            // Строка ложна, если она пустая
            return !s->get().empty();
        }
        if (auto lst = std::dynamic_pointer_cast<PyList>(obj)) {
            // Список ложен, если в нём нет элементов
            return !lst->getElements().empty();
        }
        if (auto d = std::dynamic_pointer_cast<PyDict>(obj)) {
            // Словарь ложен, если он пустой
            // repr() для пустого словаря — "{}", длина 2
            return d->repr().size() > 2;
        }
        if (auto st = std::dynamic_pointer_cast<PySet>(obj)) {
            // Множество ложено, если оно пустое
            // repr() для пустого множества — "{}", длина 2
            return st->repr().size() > 2;
        }
        if (std::dynamic_pointer_cast<PyNone>(obj)) {
            // None всегда ложь
            return false;
        }
        // Любые другие объекты (функции, пользовательские объекты и т.д.) считаются истинными
        return true;
};

auto deduceTypeName(ObjectPtr &obj) {
        if (std::dynamic_pointer_cast<PyInt>(obj))    return std::string("int");
        if (std::dynamic_pointer_cast<PyFloat>(obj))  return std::string("float");
        if (std::dynamic_pointer_cast<PyBool>(obj))   return std::string("bool");
        if (std::dynamic_pointer_cast<PyString>(obj)) return std::string("str");
        if (std::dynamic_pointer_cast<PyList>(obj))   return std::string("list");
        if (std::dynamic_pointer_cast<PyDict>(obj))   return std::string("dict");
        if (std::dynamic_pointer_cast<PySet>(obj))    return std::string("set");
        if (std::dynamic_pointer_cast<PyNone>(obj))   return std::string("NoneType");
        if (std::dynamic_pointer_cast<PyBuiltinFunction>(obj)) return std::string("builtin_function_or_method");
        if (std::dynamic_pointer_cast<PyFunction>(obj)) return std::string("function");
        // По умолчанию:
        return std::string("object");
};
 
Executor::Executor() : scopes(), reporter() {
    
    TypeRegistry::instance().registerBuiltins();

    auto print_fn = std::make_shared<PyBuiltinFunction>(
        "print",
        [](const std::vector<ObjectPtr>& args) {
            for (auto &a : args) {
                std::cout << a->repr() << " ";
            }
            std::cout << std::endl;
            return std::make_shared<PyNone>();
        }
    );
    
    scopes.insert(Symbol{"print", SymbolType::BuiltinFunction, print_fn, nullptr});

    // range(n) → [0,1,…,n-1]
    auto range_fn = std::make_shared<PyBuiltinFunction>(
        "range",
        [](const std::vector<ObjectPtr>& args) {
            auto arg0 = std::dynamic_pointer_cast<PyInt>(args[0]);
            if (!arg0) {
                throw RuntimeError("TypeError: range() argument must be int");
            }
            int n = arg0->get();
            std::vector<ObjectPtr> elems;
            elems.reserve(n);
            for (int i = 0; i < n; ++i) {
                elems.push_back(std::make_shared<PyInt>(i));
            }
            return std::make_shared<PyList>(std::move(elems));
        }
    );
    scopes.insert(Symbol{"range", SymbolType::BuiltinFunction, range_fn, nullptr});

    // 3) len(obj) — возвращает «длину» объекта
    //    Будем поддерживать: строки, списки, словари, множества. Для всего остального — ошибка.
    auto len_fn = std::make_shared<PyBuiltinFunction>(
        "len",
        [](const std::vector<ObjectPtr>& args) {
            // Проверяем, что передали ровно 1 аргумент
            if (args.size() != 1) {
                throw RuntimeError("TypeError: len() takes exactly one argument (" +
                    std::to_string(args.size()) + " given)");
            }
            ObjectPtr obj = args[0];
            // 1) Если строка
            if (auto s = std::dynamic_pointer_cast<PyString>(obj)) {
                int length = static_cast<int>(s->get().size());
                return std::make_shared<PyInt>(length);
            }
            // 2) Если список
            if (auto lst = std::dynamic_pointer_cast<PyList>(obj)) {
                int length = static_cast<int>(lst->getElements().size());
                return std::make_shared<PyInt>(length);
            }
            // 3) Если словарь
            if (auto d = std::dynamic_pointer_cast<PyDict>(obj)) {
                // repr() у словаря даёт что-то вроде "{...}", но чтобы взять
                // реальное количество элементов, лучше обратиться к внутреннему контейнеру.
                int length = static_cast<int>(d->getItems().size());
                return std::make_shared<PyInt>(length);
            }
            // 4) Если множество
            if (auto st = std::dynamic_pointer_cast<PySet>(obj)) {
                int length = static_cast<int>(st->getElements().size());
                return std::make_shared<PyInt>(length);
            }
            // 5) Если всё остальное — бросаем TypeError
            std::string tname;
            if (std::dynamic_pointer_cast<PyInt>(obj))    tname = "int";
            else if (std::dynamic_pointer_cast<PyFloat>(obj))  tname = "float";
            else if (std::dynamic_pointer_cast<PyBool>(obj))   tname = "bool";
            else if (std::dynamic_pointer_cast<PyFunction>(obj)) tname = "function";
            else tname = "object";
            throw RuntimeError("TypeError: object of type '" + tname + "' has no len()");
        }
    );
    scopes.insert(Symbol{"len", SymbolType::BuiltinFunction, len_fn, nullptr});

    // 4) dir(obj) — возвращает список «имён» элементов/ключей/символов.
    //    У нас мы сделаем так:
    //     - для списков вернём [ "0", "1", "2", ... ] (строки с индексами)
    //     - для словарей — list строковых repr() всех ключей
    //     - для строк — вернём по символам (каждый символ как строку длины 1)
    //     - для множеств — по repr() каждого элемента
    //     - для всего остального — пустой список
    auto dir_fn = std::make_shared<PyBuiltinFunction>(
        "dir",
        [](const std::vector<ObjectPtr>& args) {
            if (args.size() != 1) {
                throw RuntimeError("TypeError: dir() takes exactly one argument (" +
                    std::to_string(args.size()) + " given)");
            }
            ObjectPtr obj = args[0];
            // Результируемый вектор строк
            std::vector<ObjectPtr> names;

            // 1) Если список — возвращаем индексы в виде строк
            if (auto lst = std::dynamic_pointer_cast<PyList>(obj)) {
                int n = static_cast<int>(lst->getElements().size());
                for (int i = 0; i < n; ++i) {
                    names.push_back(std::make_shared<PyString>(std::to_string(i)));
                }
                return std::make_shared<PyList>(std::move(names));
            }
            // 2) Если словарь — возвращаем repr() ключей
            if (auto d = std::dynamic_pointer_cast<PyDict>(obj)) {
                // Получаем пары ключ->значение, но нам нужны лишь ключи
                for (auto &kv : d->getItems()) {
                    ObjectPtr keyObj = kv.first;
                    // Приводим ключ к repr() и кладём как PyString
                    names.push_back(std::make_shared<PyString>(keyObj->repr()));
                }
                return std::make_shared<PyList>(std::move(names));
            }
            // 3) Если строка — возвращаем по одному символу
            if (auto s = std::dynamic_pointer_cast<PyString>(obj)) {
                const std::string &str = s->get();
                for (char c : str) {
                    std::string ch(1, c);
                    names.push_back(std::make_shared<PyString>(ch));
                }
                return std::make_shared<PyList>(std::move(names));
            }
            // 4) Если множество — возвращаем repr() каждого элемента
            if (auto st = std::dynamic_pointer_cast<PySet>(obj)) {
                for (auto &el : st->getElements()) {
                    names.push_back(std::make_shared<PyString>(el->repr()));
                }
                return std::make_shared<PyList>(std::move(names));
            }
            // 5) Во всех остальных случаях возвращаем пустой список
            return std::make_shared<PyList>(std::vector<ObjectPtr>{});
        }
    );
    scopes.insert(Symbol{"dir", SymbolType::BuiltinFunction, dir_fn, nullptr});

    // 5) enumerate(iterable) — возвращаем список пар [index, element].
    //    Реализуем для списков, строк и словарей (по ключам). Во всех остальных — TypeError.
    auto enumerate_fn = std::make_shared<PyBuiltinFunction>(
        "enumerate",
        [](const std::vector<ObjectPtr>& args) {
            if (args.size() != 1) {
                throw RuntimeError("TypeError: enumerate() takes exactly one argument (" +
                    std::to_string(args.size()) + " given)");
            }
            ObjectPtr obj = args[0];
            std::vector<ObjectPtr> result;  // Будем класть сюда серии [индекс, элемент]

            // 1) Если список
            if (auto lst = std::dynamic_pointer_cast<PyList>(obj)) {
                const auto &elems = lst->getElements();
                for (size_t i = 0; i < elems.size(); ++i) {
                    // Составляем пару [ i, elems[i] ]
                    std::vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(std::make_shared<PyInt>(static_cast<int>(i)));
                    pairVec.push_back(elems[i]);
                    // Сами пары будем хранить как PyList из двух элементов
                    result.push_back(std::make_shared<PyList>(std::move(pairVec)));
                }
                return std::make_shared<PyList>(std::move(result));
            }
            // 2) Если строка — итерируем по символам
            if (auto s = std::dynamic_pointer_cast<PyString>(obj)) {
                const std::string &str = s->get();
                for (size_t i = 0; i < str.size(); ++i) {
                    std::vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(std::make_shared<PyInt>(static_cast<int>(i)));
                    // Второй элемент — символ как PyString
                    pairVec.push_back(std::make_shared<PyString>(std::string(1, str[i])));
                    result.push_back(std::make_shared<PyList>(std::move(pairVec)));
                }
                return std::make_shared<PyList>(std::move(result));
            }
            // 3) Если словарь — итерируем по ключам, возвращаем пары [index, key]
            if (auto d = std::dynamic_pointer_cast<PyDict>(obj)) {
                const auto &items = d->getItems();
                size_t idx = 0;
                for (auto &kv : items) {
                    ObjectPtr keyObj = kv.first;
                    std::vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(std::make_shared<PyInt>(static_cast<int>(idx)));
                    pairVec.push_back(keyObj);
                    result.push_back(std::make_shared<PyList>(std::move(pairVec)));
                    ++idx;
                }
                return std::make_shared<PyList>(std::move(result));
            }
            // 4) В остальных случаях — ошибка типа
            std::string tname;
            if (std::dynamic_pointer_cast<PyInt>(obj))    tname = "int";
            else if (std::dynamic_pointer_cast<PyFloat>(obj))  tname = "float";
            else if (std::dynamic_pointer_cast<PyBool>(obj))   tname = "bool";
            else tname = "object";
            throw RuntimeError("TypeError: '" + tname + "' object is not iterable");
        }
    );
    scopes.insert(Symbol{"enumerate", SymbolType::BuiltinFunction, enumerate_fn, nullptr});
}

std::shared_ptr<Object> Executor::evaluate(Expression &expr) {
    expr.accept(*this);
    return pop_value();
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
    // 1) Проверка дубликатов имён параметров
    {
        std::unordered_set<std::string> seen;
        // позиционные параметры
        for (const auto &name : node.posParams) {
            if (!seen.insert(name).second) {
                reporter.add_error(
                    "Line " + std::to_string(node.line)
                    + ": duplicate parameter name '" + name 
                    + "' in function '" + node.name + "'"
                );
            }
        }
        // параметры с default-значением
        for (const auto &pr : node.defaultParams) {
            const std::string &name = pr.first;
            if (!seen.insert(name).second) {
                reporter.add_error(
                    "Line " + std::to_string(node.line)
                    + ": duplicate parameter name '" + name 
                    + "' in function '" + node.name + "'"
                );
            }
        }
    }

    // 2) Если ошибки появились — прекращаем обработку
    if (reporter.has_errors()) {
        return;
    }

    // 3) Вычисляем default-значения прямо сейчас, в момент объявления функции.
    //    Эти значения сохранятся в объекте PyFunction как «фиксированные».
    std::vector<ObjectPtr> default_values;
    default_values.reserve(node.defaultParams.size());
    for (auto &pr : node.defaultParams) {
        Expression *expr = pr.second.get();
        if (expr) {
            // Вычисляем выражение
            expr->accept(*this);
            // Берём результат из стека
            ObjectPtr value = pop_value();
            default_values.push_back(value);
        } else {
            // Если вдруг передано nullptr, просто кладём PyNone
            default_values.push_back(std::make_shared<PyNone>());
        }
    }

    // 4) Создаём объект пользовательской функции,
    //    передаём ему:
    //      - имя
    //      - указатель на AST-узел FuncDecl (чтобы знать тело функции)
    //      - «лексическое окружение» (environment) — таблицу символов текущего scope
    //      - список имён позиционных параметров
    //      - вектор default-значений
    auto fn_obj = std::make_shared<PyFunction>(
        node.name,
        &node,
        scopes.currentTable(),  // лексическое окружение, чтобы функция могла замыкать переменные
        node.posParams,
        std::move(default_values)
    );

    // 5) Вставляем этот объект в таблицу символов текущей области видимости
    //    и помечаем тип SymbolType::Function.
    Symbol sym;
    sym.name  = node.name;
    sym.type  = SymbolType::Function;
    sym.value = fn_obj;
    sym.decl  = &node;

    if (auto existing = scopes.lookup_local(node.name)) {
        // Если имя функции уже было в этом же scope (например, другая функция
        // с тем же именем), перезаписываем символ
        *existing = sym;
    } else {
        scopes.insert(sym);
    }

    // Мы явно НЕ enter_scope() и не leave_scope(), 
    // потому что мы не входили «внутрь» тела функции в момент объявления.
    // Фактический «вход в тело» произойдет, когда кто-то вызовет эту функцию.
}

void Executor::visit(BlockStat &node) {
    for (auto &stat : node.statements) {
        stat->accept(*this);
        
        if (reporter.has_errors()) {
            return;
        }
    }
}

// ExprStat: вычисляем выражение; если выражение возвращает какой-то объект,
// но мы его не используем дальше (потому что это просто «выражение в виде
// инструкции»), то «попаем» его из стека и просто забываем.
// Исключение: если это вызов функции-билтинга вроде print, то print сам
// выведет что надо и вернёт PyNone, которое мы всё равно сбросим.

void Executor::visit(ExprStat &node) {
    if (node.expr) {
        node.expr->accept(*this);
        // Одно значение появилось в valueStack
        // Сбрасываем его (мы не используем результат выражения дальше, 
        // т.к. это просто «выражение-инструкция»).
        pop_value();
    }
}

// AssignStat: обрабатываем три случая:
//   1) левый IdExpr  →  обычное присваивание в текущем scope
//   2) левый IndexExpr   →  присваивание по индексу (list[i] = …, dict[key] = …)
//   3) левый AttributeExpr →  присваивание атрибута (obj.field = …).


void Executor::visit(AssignStat &node) {
    // 1) Вычисляем правую часть присваивания (справа от '='):
    if (node.right) {
        node.right->accept(*this);
    } else {
        // Если вдруг node.right == nullptr, значит присваивается «ничего» —
        // можно считать, что это эквивалентно None.
        push_value(std::make_shared<PyNone>());
    }
    // Теперь на вершине стека хранится ObjectPtr, которое мы собираемся присвоить
    ObjectPtr right_val = pop_value();

    // 2) Разбираем левую часть:
    // 2.1) Если просто идентификатор: IdExpr
    if (auto id = dynamic_cast<IdExpr*>(node.left.get())) {
        std::string var = id->name;

        // Проверяем, есть ли уже локальный символ с таким именем:
        if (!scopes.lookup_local(var)) {
            // Если нет — создаём новую переменную в этом scope
            Symbol sym;
            sym.name  = var;
            sym.type  = SymbolType::Variable;
            sym.value = nullptr;     // пока просто задаём nullptr, заполнится ниже
            sym.decl  = &node;
            scopes.insert(sym);
        }
        // Теперь у нас точно есть Symbol* в текущем локальном scope:
        Symbol *sym = scopes.lookup_local(var);
        // Кладём внутрь переменной вычисленное значение:
        sym->value = right_val;
        return;
    }

    // 2.2) Если индексное присваивание: left это IndexExpr
    if (auto idx = dynamic_cast<IndexExpr*>(node.left.get())) {
        // Сначала вычисляем «базу»: допустим, это список или словарь
        idx->base->accept(*this);
        ObjectPtr base = pop_value();

        // Затем вычисляем «индекс» (ключ или целый индекс)
        idx->index->accept(*this);
        ObjectPtr index = pop_value();

        // Теперь пытаемся выполнить присваивание base[index] = right_val.
        // Для этого мы проверяем, поддерживает ли base метод __setitem__.
        // Допустим, у нас в классе Object есть виртуальный метод
        //   virtual void __setitem__(ObjectPtr key, ObjectPtr value) { ... }
        // и у конкретных подклассов (PyList, PyDict, …) он переопределён.
        // Если метод у base не переопределён, то бросаем ошибку.

        // Пример (предположим, что мы добавили виртуальный __setitem__ в Object):
        try {
            base->__setitem__(index, right_val);
        }
        catch (const RuntimeError &err) {
            // Если базовый объект не поддерживает __setitem__, __setitem__ 
            // сам кидает соответствующий RuntimeError с понятным сообщением.
            throw;
        }
        return;
    }

    // 2.3) Если атрибутное присваивание: left это AttributeExpr
    if (auto attr = dynamic_cast<AttributeExpr*>(node.left.get())) {
        // Сначала вычисляем объект, у которого устанавливаем атрибут:
        attr->obj->accept(*this);
        ObjectPtr base = pop_value();

        // Имя атрибута:
        std::string field = attr->name;

        // Если у нас в Object есть виртуальный метод setattr(name, ObjectPtr),
        // то просто вызываем его. Если нет — бросаем ошибку.
        try {
            base->__setattr__(field, right_val);
        }
        catch (const RuntimeError &err) {
            throw;
        }
        return;
    }

    // 2.4) Во всех остальных случаях — недопустимое левое значение (синтаксис мы,
    // по идее, уже проверили на этапе парсера, но на всякий случай):
    throw RuntimeError(
        "Line " + std::to_string(node.line)
        + ": invalid assignment target"
    );
}

void Executor::visit(IdExpr &node) {
    // При встрече идентификатора нужно извлечь его текущее значение из таблицы символов
    // и сохранить его в «текущем значении» (либо на стек выражений, в зависимости от реализации).

    // 1) Ищем символ в текущем (или во внешних) областях видимости
    Symbol* sym = scopes.lookup(node.name);

    // 2) Если символ не найден — это ошибка времени выполнения
    if (!sym) {
        throw RuntimeError(
            "Line " + std::to_string(node.line) + ": name '" + node.name + "' is not defined"
        );
    }

    // 3) Если символ найден, но у него нет присвоенного значения,
    //    то это тоже ошибка (переменная объявлена, но не инициализирована).
    if (!sym->value) {
        throw RuntimeError(
            "Line " + std::to_string(node.line) + ": variable '" + node.name + "' referenced before assignment"
        );
    }

    // 4) Извлекаем объект и сохраняем его как «результат» этого узла AST.
    //    Предполагаем, что в Executor есть метод push_value, который запоминает
    //    результат вычисления текущего поддерева.
    push_value(sym->value);
}


void Executor::visit(BinaryExpr &node) {
    // 1) Вычисляем левый операнд
    node.left->accept(*this);
    ObjectPtr leftVal = pop_value();

    // 2) Вычисляем правый операнд
    node.right->accept(*this);
    ObjectPtr rightVal = pop_value();

    // 3) Обрабатываем оператор
    const std::string &op = node.op;
    ObjectPtr result;

    if (op == "+") {
        result = leftVal->__add__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "-") {
        result = leftVal->__sub__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "*") {
        result = leftVal->__mul__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "/") {
        result = leftVal->__div__(rightVal);
        push_value(result);
        return;
    }

    // Сравнения
    else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        std::string lhs = leftVal->repr();
        std::string rhs = rightVal->repr();
        bool comp = false;
        if (op == "==") comp = (lhs == rhs);
        else if (op == "!=") comp = (lhs != rhs);
        else if (op == "<") comp = (lhs < rhs);
        else if (op == ">") comp = (lhs > rhs);
        else if (op == "<=") comp = (lhs <= rhs);
        else if (op == ">=") comp = (lhs >= rhs);
        push_value(std::make_shared<PyBool>(comp));
        return;
    }

    // Логические
    else if (op == "and" || op == "or") {

        bool leftTruthy = is_truthy(leftVal);
        if (op == "and") {
            if (!leftTruthy) {
                push_value(leftVal);
                return;
            } else {
                push_value(rightVal);
                return;
            }
        } else {
            if (leftTruthy) {
                push_value(leftVal);
                return;
            } else {
                push_value(rightVal);
                return;
            }
        }
    }

    // in / not in
    else if (op == "in" || op == "not in") {
        bool containsResult = false;
        try {
            containsResult = rightVal->__contains__(leftVal);
        } catch (const RuntimeError &err) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: " + err.what()
            );
        }
        if (op == "not in") {
            containsResult = !containsResult;
        }
        push_value(std::make_shared<PyBool>(containsResult));
        return;
    }

    throw RuntimeError(
        "Line " + std::to_string(node.line)
        + ": unsupported binary operator '" + op + "'"
    );
}


void Executor::visit(UnaryExpr &node) {
    // 1) Сначала вычисляем операнд
    if (node.operand) {
        node.operand->accept(*this);
    } else {
        // На всякий случай: если operand == nullptr, считаем это ошибкой
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": missing operand for unary operator '" + node.op + "'"
        );
    }

    ObjectPtr operandVal = pop_value();

    const std::string &op = node.op;
    // Определим «тип» объекта в виде строки для сообщений об ошибках

    // --- unary + ---
    if (op == "+") {
        // В Python «+x» достаточно просто вернуть x, если x – число или булево
        if (auto pint = std::dynamic_pointer_cast<PyInt>(operandVal)) {
            // Можно вернуть тот же объект или создать новый – оставим тот же
            push_value(pint);
            return;
        }
        if (auto pfloat = std::dynamic_pointer_cast<PyFloat>(operandVal)) {
            push_value(pfloat);
            return;
        }
        if (auto pbool = std::dynamic_pointer_cast<PyBool>(operandVal)) {
            // Булево тоже интерпретируется как число 0/1 → возвращаем PyInt или PyBool? Лучше PyInt
            int intval = pbool->get() ? 1 : 0;
            push_value(std::make_shared<PyInt>(intval));
            return;
        }
        // Для остальных типов + недопустимо
        std::string tname = deduceTypeName(operandVal);
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: bad operand type for unary +: '" + tname + "'"
        );
    }

    // --- unary - ---
    else if (op == "-") {
        if (auto pint = std::dynamic_pointer_cast<PyInt>(operandVal)) {
            push_value(std::make_shared<PyInt>(- pint->get()));
            return;
        }
        if (auto pfloat = std::dynamic_pointer_cast<PyFloat>(operandVal)) {
            push_value(std::make_shared<PyFloat>(- pfloat->get()));
            return;
        }
        if (auto pbool = std::dynamic_pointer_cast<PyBool>(operandVal)) {
            // Булево как число: True → 1, False → 0
            int intval = pbool->get() ? 1 : 0;
            push_value(std::make_shared<PyInt>(- intval));
            return;
        }
        // Все остальное – ошибка
        std::string tname = deduceTypeName(operandVal);
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: bad operand type for unary -: '" + tname + "'"
        );
    }

    // --- logical not ---
    else if (op == "not") {
        bool truth = is_truthy(operandVal);
        push_value(std::make_shared<PyBool>(!truth));
        return;
    }

    // --- недопусточный оператор ---
    else {
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " SyntaxError: invalid unary operator '" + op + "'"
        );
    }
}


void Executor::visit(CallExpr &node) {
    // 1) Сначала вычисляем «вызывающий» (callee). Это может быть либо
    //    идентификатор функции (IdExpr), либо более сложное выражение,
    //    возвращающее объект, поддерживающий вызов (__call__).
    node.caller->accept(*this);
    ObjectPtr callee = pop_value();

    // 2) Затем вычисляем все аргументы в том же порядке, в каком они указаны в AST.
    std::vector<ObjectPtr> args;
    args.reserve(node.arguments.size());
    for (auto &argExpr : node.arguments) {
        argExpr->accept(*this);
        args.push_back(pop_value());
    }

    // 3) Теперь нам нужно проверить, «вызываемый объект» (callee) действительно
    //    является вызываемым. В Python любая функция (builtin или user) предоставляет
    //    метод __call__. Если это не функция и вообще не вызывает __call__, бросим ошибку.

    // --- Сначала обрабатываем встроенные функции (PyBuiltinFunction) ---
    if (auto builtinFn = std::dynamic_pointer_cast<PyBuiltinFunction>(callee)) {
        // Просто вызываем __call__ у встроенной функции
        // Внутри __call__ для PyBuiltinFunction уже реализована логика,
        // вывод через std::cout и возврат PyNone или любого другого объекта.
        ObjectPtr result;
        try {
            result = builtinFn->__call__(args);
        }
        catch (const RuntimeError &err) {
            // Если внутри встроенной функции произошла ошибка,
            // просто пробрасываем дальше с указанием строки.
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " " + err.what()
            );
        }
        // Кладём результат на стек
        push_value(result);
        return;
    }

    // --- Теперь проверим, может быть это пользовательская функция (PyFunction) ---
    if (auto userFn = std::dynamic_pointer_cast<PyFunction>(callee)) {
        // Извлекаем AST-узел FuncDecl, который описывает тело функции
        FuncDecl *decl = userFn->getDecl();

        // 3.1) Проверяем число аргументов:
        size_t provided = args.size();
        size_t requiredPos = decl->posParams.size();
        size_t defaultCount = decl->defaultParams.size();

        // Если передали меньше, чем позиционных (обязательных) параметров — ошибка
        if (provided < requiredPos) {
            size_t missing = requiredPos - provided;
            // Соберём имена тех параметров, которые не были переданы
            std::vector<std::string> missingNames;
            for (size_t i = provided; i < requiredPos; ++i) {
                missingNames.push_back("'" + decl->posParams[i] + "'");
            }
            std::string howMany = (missing == 1 ? "1 required positional argument" : 
                                   std::to_string(missing) + " required positional arguments");
            std::string namesList;
            // Соединяем имена через «and» (как в Python)
            if (missingNames.size() == 1) {
                namesList = missingNames[0];
            } else {
                for (size_t i = 0; i < missingNames.size(); ++i) {
                    namesList += missingNames[i];
                    if (i + 1 < missingNames.size())
                        namesList += " and ";
                }
            }
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: " + decl->name + "() missing " 
                + howMany + ": " + namesList
            );
        }

        // Если передали больше, чем позиционных + default, тоже ошибка
        if (provided > requiredPos + defaultCount) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: " + decl->name + "() takes from " 
                + std::to_string(requiredPos) + " to " 
                + std::to_string(requiredPos + defaultCount) 
                + " positional arguments but " + std::to_string(provided) 
                + " were given"
            );
        }

        // 3.2) Создаём новую таблицу символов, «дочернюю» от лексического окружения.
        //      В ней будут локальные переменные и параметры функции.
        scopes.enter_scope();

        // 3.3) Сначала «привязываем» все позиционные параметры:
        //      Позиционные параметры — это decl->posParams[i], i = 0..requiredPos-1.
        for (size_t i = 0; i < requiredPos; ++i) {
            Symbol sym;
            sym.name = decl->posParams[i];
            sym.type = SymbolType::Parameter;
            sym.value = args[i];
            sym.decl = decl; // чтобы знать, что это параметр этой функции
            scopes.insert(sym);
        }

        // 3.4) Теперь обрабатываем параметры с default-значениями.
        //      decl->defaultParams — вектор пар (имя параметра, AST-узел Expression),
        //      а userFn хранит уже вычисленные default-значения в том же порядке
        //      (в userFn->getDefaultValues()).
        //      Если для default-параметра i передан аргумент (то есть provided > requiredPos + i),
        //      мы используем args[requiredPos + i], иначе — defaultValues[i].
        const auto &defParams = decl->defaultParams;                // в AST
        const auto &defValues = userFn->getDefaultValues();         // в PyFunction
        for (size_t i = 0; i < defParams.size(); ++i) {
            const std::string &paramName = defParams[i].first;
            ObjectPtr valueToBind;
            size_t argIndex = requiredPos + i;
            if (argIndex < provided) {
                // Был передан позиционный аргумент, «перекрывающий» default
                valueToBind = args[argIndex];
            } else {
                // Позиционные кончились — используем заранее вычисленное default
                valueToBind = defValues[i];
            }
            Symbol sym;
            sym.name = paramName;
            sym.type = SymbolType::Parameter;
            sym.value = valueToBind;
            sym.decl = decl;
            scopes.insert(sym);
        }

        // 4) Теперь выполняем тело функции. Если внутри встретится ReturnStat,
        //    мы из visit(ReturnStat) кинем ReturnException, и здесь его поймаем.
        ObjectPtr returnValue = std::make_shared<PyNone>(); // по умолчанию None
        try {
            if (decl->body) {
                decl->body->accept(*this);
            }
            // Если дошли сюда без «return», возвращаем PyNone
        }
        catch (const ReturnException &ret) {
            // Если пользовательская функция вызвала return X — мы тут получим X
            returnValue = ret.value;
        }

        // 5) В любом случае «выходим» из локальной области видимости
        scopes.leave_scope();

        // 6) Кладём полученный returnValue на стек и возвращаемся
        push_value(returnValue);
        return;
    }

    // --- НИ ОДИН ИЗ ТИПОВ ВЫЗЫВАЕМЫХ ФУНКЦИЙ НЕ ПОДОШЁЛ ---
    // Попытаемся просто вызвать __call__ (на случай, если это любая другая
    // callable-реализация через переопределение __call__ в Object). Если она бросит,
    // мы «пробросим» ошибку как TypeError.
    try {
        ObjectPtr result = callee->__call__(args);
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: " + err.what()
        );
    }
}

// Не забываем: внутри Visit(ReturnStat &node) нужно бросать ReturnException
// чтобы «прерваться» и вернуть нужное значение:
void Executor::visit(ReturnStat &node) {
    // Если есть выражение, вычисляем его; иначе return без значения → None
    if (node.expr) {
        node.expr->accept(*this);
        ObjectPtr retVal = pop_value();
        throw ReturnException{retVal};
    } else {
        throw ReturnException{ std::make_shared<PyNone>() };
    }
}   

void Executor::visit(IndexExpr &node) {
    // 1) Сначала вычисляем «базу»: obj
    node.base->accept(*this);
    ObjectPtr baseVal = pop_value();

    // 2) Затем вычисляем «индекс»: idx
    node.index->accept(*this);
    ObjectPtr indexVal = pop_value();

    // 3) Попробуем определить, какого типа «базовый» объект,
    //    и применим правила Python для индексирования.
    if (!baseVal) {
    throw RuntimeError("Line ...: internal error: base is null");
    }
    if (!indexVal) {
    throw RuntimeError("Line ...: internal error: index is null");
    }

    // --- 3.1) Если это строка: разрешаем только целочисленные индексы (bool → int тоже)
    if (auto strObj = std::dynamic_pointer_cast<PyString>(baseVal)) {
        // Проверяем, что индекс — PyInt или PyBool
        int idx;
        if (auto intIdx = std::dynamic_pointer_cast<PyInt>(indexVal)) {
            idx = intIdx->get();
        }
        else if (auto boolIdx = std::dynamic_pointer_cast<PyBool>(indexVal)) {
            idx = boolIdx->get() ? 1 : 0;
        }
        else {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: string indices must be integers"
            );
        }

        const std::string &s = strObj->get();
        int length = static_cast<int>(s.size());

        // В Python разрешены отрицательные индексы
        if (idx < 0) {
            idx += length;
        }

        // Проверяем диапазон
        if (idx < 0 || idx >= length) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " IndexError: string index out of range"
            );
        }

        // Получаем символ и возвращаем как строку длины 1
        char c = s[idx];
        push_value(std::make_shared<PyString>(std::string(1, c)));
        return;
    }

    // --- 3.2) Если это список: разрешаем целочисленные индексы (bool → int тоже)
    if (auto listObj = std::dynamic_pointer_cast<PyList>(baseVal)) {
        int idx;
        if (auto intIdx = std::dynamic_pointer_cast<PyInt>(indexVal)) {
            idx = intIdx->get();
        }
        else if (auto boolIdx = std::dynamic_pointer_cast<PyBool>(indexVal)) {
            idx = boolIdx->get() ? 1 : 0;
        }
        else {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: list indices must be integers"
            );
        }

        int length = static_cast<int>(listObj->getElements().size());
        if (idx < 0) {
            idx += length;
        }
        if (idx < 0 || idx >= length) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " IndexError: list index out of range"
            );
        }

        // Получаем элемент и возвращаем
        ObjectPtr element = listObj->getElements()[idx];
        push_value(element);
        return;
    }

    // --- 3.3) Если это словарь: index может быть любым объектом.
    if (auto dictObj = std::dynamic_pointer_cast<PyDict>(baseVal)) {
        // Просто вызываем __getitem__; если ключа нет — KeyError бросят там
        try {
            ObjectPtr result = dictObj->__getitem__(indexVal);
            push_value(result);
            return;
        }
        catch (const RuntimeError &err) {
            // Сообщение от PyDict: "KeyError: <repr(key)>"
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " " + err.what()
            );
        }
    }

    // --- 3.4) Если это множество: в Python множеству нельзя делать индексирование
    if (std::dynamic_pointer_cast<PySet>(baseVal)) {
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: 'set' object is not subscriptable"
        );
    }

    // --- 3.5) Для любых других объектов пробуем вызвать __getitem__ напрямую.
    //        Если __getitem__ не поддерживается, он бросит RuntimeError("object is not subscriptable").
    try {
        ObjectPtr result = baseVal->__getitem__(indexVal);
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        // Добавляем информацию о строке
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: " + err.what()
        );
    }
}



void Executor::visit(AttributeExpr &node) {
    // 1) Сначала вычисляем «объект», у которого хотим взять атрибут.
    //    Например, в выражении a.b.c сначала вычислим a.b, а потом .c.
    node.obj->accept(*this);

    if (!node.obj) {
    throw RuntimeError("Line ...: internal error: AttributeExpr.obj is null");
    }

    ObjectPtr baseVal = pop_value();

    if (!baseVal) {
    throw RuntimeError("Line ...: internal error: baseVal is null in AttributeExpr");
    }
    // 2) Имя атрибута, которое запрашиваем.
    const std::string &attrName = node.name;

    // 3) Пытаемся получить атрибут через виртуальный метод __getattr__.
    //    Если объект не поддерживает данный атрибут, __getattr__ бросит RuntimeError
    //    с сообщением "object has no attribute '...'" или подобным.
    try {
        ObjectPtr result = baseVal->__getattr__(attrName);
        // 4) Если всё прошло успешно, кладём найденный атрибут на стек значений
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        // 5) Если __getattr__ бросил ошибку, превращаем её в TypeError с указанием строки
        //    Python-стиль: "Line X TypeError: 'TYPE' object has no attribute 'attrName'"
        //    Здесь err.what() уже содержит текст типа "object has no attribute 'foo'",
        //    но лучше скорректировать, чтобы было точно в формате Python.
        //
        //    Сначала определим, какого «типа» объект, чтобы сообщение было понятнее.
        std::string typeName;
        if (std::dynamic_pointer_cast<PyInt>(baseVal)) {
            typeName = "int";
        }
        else if (std::dynamic_pointer_cast<PyFloat>(baseVal)) {
            typeName = "float";
        }
        else if (std::dynamic_pointer_cast<PyBool>(baseVal)) {
            typeName = "bool";
        }
        else if (std::dynamic_pointer_cast<PyString>(baseVal)) {
            typeName = "str";
        }
        else if (std::dynamic_pointer_cast<PyList>(baseVal)) {
            typeName = "list";
        }
        else if (std::dynamic_pointer_cast<PyDict>(baseVal)) {
            typeName = "dict";
        }
        else if (std::dynamic_pointer_cast<PySet>(baseVal)) {
            typeName = "set";
        }
        else if (std::dynamic_pointer_cast<PyNone>(baseVal)) {
            typeName = "NoneType";
        }
        else if (std::dynamic_pointer_cast<PyBuiltinFunction>(baseVal)) {
            typeName = "builtin_function_or_method";
        }
        else if (std::dynamic_pointer_cast<PyFunction>(baseVal)) {
            typeName = "function";
        }
        else {
            // На всякий случай, если это какой-то свой объект
            typeName = "object";
        }

        // Формируем текст ошибки в духе Python:
        // "Line X TypeError: 'TYPE' object has no attribute 'attrName'"
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: '" + typeName
            + "' object has no attribute '" + attrName + "'"
        );
    }
}


void Executor::visit(TernaryExpr &node) {
    // В Python тернарный оператор выглядит так: <expr1> if <condition> else <expr2>
    // Нужно сначала вычислить условие и определить его truthiness,
    // а затем – в зависимости от результата – вычислить либо выражение после if, либо после else.
    
    // 1) Сначала вычисляем условие
    if (!node.condition) {
        // На всякий случай: если вдруг нет узла condition, считаем это ошибкой
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": missing condition in ternary expression"
        );
    }
    node.condition->accept(*this);
    // Сохраним результат в локальную переменную
    ObjectPtr condVal = pop_value();
    
    // 2) Определяем truthiness для condVal.
    //    В Python считается «ложным»: False, 0, 0.0, "", [], {}, set(), None.
    //    Во всех остальных случаях – истинным.
    
    bool condIsTrue = is_truthy(condVal);
    
    // 3) В зависимости от condIsTrue вычисляем нужный подвыражение:
    if (condIsTrue) {
        // Если условие истинно – вычисляем trueExpr
        if (!node.trueExpr) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": missing expression after 'if' in ternary"
            );
        }
        node.trueExpr->accept(*this);
        ObjectPtr trueResult = pop_value();
        // Кладём результат trueExpr на стек
        push_value(trueResult);
    }
    else {
        // Иначе – вычисляем falseExpr
        if (!node.falseExpr) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": missing expression after 'else' in ternary"
            );
        }
        node.falseExpr->accept(*this);
        ObjectPtr falseResult = pop_value();
        // Кладём результат falseExpr на стек
        push_value(falseResult);
    }
}



void Executor::visit(LiteralExpr &node) {
    // В LiteralExpr хранятся примитивные литералы: int, double, bool, string или None.
    // Нужно распознать, какой тип литерала и положить соответствующий объект на стек.

    // Предполагаем, что в классе LiteralExpr объявлено нечто вроде:
    //   std::variant<std::monostate, int, double, bool, std::string> value;
    // где:
    //   - std::monostate обозначает литерал "None"
    //   - int обозначает целое число
    //   - double обозначает вещественное число
    //   - bool обозначает литерал True/False
    //   - std::string обозначает строковый литерал (без кавычек, уже распарсенный)

    // Проверяем, какой вариант хранится в node.value:

    // 1) None
    if (std::holds_alternative<std::monostate>(node.value)) {
        // Литерал None
        ObjectPtr noneObj = std::make_shared<PyNone>();
        push_value(noneObj);
        return;
    }

    // 2) Целое число
    if (std::holds_alternative<int>(node.value)) {
        int intVal = std::get<int>(node.value);
        ObjectPtr intObj = std::make_shared<PyInt>(intVal);
        push_value(intObj);
        return;
    }

    // 3) Вещественное число (float)
    if (std::holds_alternative<double>(node.value)) {
        double dblVal = std::get<double>(node.value);
        ObjectPtr floatObj = std::make_shared<PyFloat>(dblVal);
        push_value(floatObj);
        return;
    }

    // 4) Логическое значение (bool)
    if (std::holds_alternative<bool>(node.value)) {
        bool boolVal = std::get<bool>(node.value);
        ObjectPtr boolObj = std::make_shared<PyBool>(boolVal);
        push_value(boolObj);
        return;
    }

    // 5) Строковый литерал
    if (std::holds_alternative<std::string>(node.value)) {
        // Предполагаем, что парсер убрал кавычки, и здесь передана «сырая» строка.
        std::string strVal = std::get<std::string>(node.value);
        ObjectPtr strObj = std::make_shared<PyString>(strVal);
        push_value(strObj);
        return;
    }

    // Если ни один тип не подошёл (например, в variant проскочил неизвестный тип),
    // то лучше бросить ошибку на всякий случай.
    throw RuntimeError(
        "Line " + std::to_string(node.line)
        + ": unsupported literal type"
    );
}

void Executor::visit(PrimaryExpr &node) {
    // В PrimaryExpr может быть один из пяти подтипов: 
    //   LITERAL, ID, CALL, INDEX, PAREN, TERNARY.
    // Мы просто перенаправляем на соответствующий узел AST.

    switch (node.type) {
        case PrimaryExpr::PrimaryType::LITERAL:
            // Если это литерал, то внутри node.literalExpr хранится
            // указатель на LiteralExpr. Проверяем, что он не nullptr, и вызываем accept.
            if (!node.literalExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::LITERAL has no literalExpr"
                );
            }
            node.literalExpr->accept(*this);
            break;

        case PrimaryExpr::PrimaryType::ID:
            // Если это идентификатор
            if (!node.idExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::ID has no idExpr"
                );
            }
            node.idExpr->accept(*this);
            break;

        case PrimaryExpr::PrimaryType::CALL:
            // Если это вызов функции
            if (!node.callExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::CALL has no callExpr"
                );
            }
            node.callExpr->accept(*this);
            break;

        case PrimaryExpr::PrimaryType::INDEX:
            // Если это индексирование: obj[idx]
            if (!node.indexExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::INDEX has no indexExpr"
                );
            }
            node.indexExpr->accept(*this);
            break;

        case PrimaryExpr::PrimaryType::PAREN:
            // Если это скобочное выражение, например: (a + b)
            if (!node.parenExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::PAREN has no parenExpr"
                );
            }
            // Просто делегируем внутрь
            node.parenExpr->accept(*this);
            break;

        case PrimaryExpr::PrimaryType::TERNARY:
            // Тоже может быть тернарный оператор внутри PrimaryExpr
            if (!node.ternaryExpr) {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: PrimaryExpr::TERNARY has no ternaryExpr"
                );
            }
            node.ternaryExpr->accept(*this);
            break;

        default:
            // На всякий случай: если вдруг добавят новый тип, а мы его не обработали
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": unsupported primary expression type"
            );
    }
}


void Executor::visit(ListExpr &node) {
    // В Python список создаётся как новый объект, в котором хранятся результаты
    // вычисления каждого элемента в квадратных скобках, например: [a, b + 1, foo()].
    // Здесь важно:
    //  1) вычислить каждый элемент слева направо,
    //  2) положить результат вычисления каждого элемента в вектор ObjectPtr,
    //  3) создать новый PyList на основе этого вектора,
    //  4) положить полученный объект списка на стек значений.

    // 1) Создаём временный вектор, в который будем накапливать ObjectPtr
    //    для каждого элемента списка.
    std::vector<ObjectPtr> elements;
    // Делаем резервирование памяти сразу под нужное количество элементов,
    // чтобы не пересоздавать вектор лишний раз.
    elements.reserve(node.elems.size());

    // 2) Проходим по каждому подвыражению в node.elems
    for (size_t i = 0; i < node.elems.size(); ++i) {
        auto &elemExpr = node.elems[i];
        if (elemExpr) {
            // 2.1) Если указатель на Expression ненулевой, вычисляем это выражение.
            elemExpr->accept(*this);
            // 2.2) После accept на вершине стека окажется ObjectPtr,
            //      который соответствует значению этого выражения.
            ObjectPtr value = pop_value();
            // 2.3) Кладём полученное значение в наш временный вектор.
            elements.push_back(value);
        } else {
            // 2.4) На всякий случай: если вдруг есть nullptr (пустой узел),
            //      то в Python был бы синтаксический элемент вроде «[,]» без выражения.
            //      Обычно такого не бывает, но мы можем положить None, чтобы не сломаться.
            elements.push_back(std::make_shared<PyNone>());
        }
    }

    // 3) После того, как все элементы обработаны и лежат в векторе elements,
    //    создаём объект PyList с копией (или перемещением) нашего вектора.
    ObjectPtr listObj = std::make_shared<PyList>(std::move(elements));

    // 4) Кладём полученный объект списка на стек значений:
    push_value(listObj);
}



void Executor::visit(DictExpr &node) {
    // В Python словарь создаётся как набор пар "ключ: значение", например:
    // {k1: v1, k2: v2, …}. При создании:
    //  1) вычисляются ключ и значение для каждой пары в порядке слева направо,
    //  2) создаётся новый объект PyDict,
    //  3) вызывается __setitem__ для каждой пары, чтобы добавить (или обновить) запись,
    //  4) если во время вычисления ключей или значений или при вставке возникнет ошибка,
    //     нужно выбросить RuntimeError с указанием номера строки в стиле Python.

    // 1) Создаём новый пустой словарь
    auto dictObj = std::make_shared<PyDict>();

    // 2) Пробегаем по всем парам (ключ, значение) в узле AST
    //    Предположим, что node.items — это вектор пар:
    //       std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
    //    где first — узел для ключа, second — узел для значения.
    for (size_t i = 0; i < node.items.size(); ++i) {
        auto &keyExpr   = node.items[i].first;   // узел Expression для ключа
        auto &valueExpr = node.items[i].second;  // узел Expression для значения

        // 2.1) Сначала вычисляем выражение ключа
        if (keyExpr) {
            keyExpr->accept(*this);
        } else {
            // На всякий случай, если вдруг ключ отсутствует (nullptr),
            // считаем это внутренней ошибкой — но Python бы просто упал на этапе парсинга.
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: missing key expression in dict literal"
            );
        }
        // После accept() на вершине стека окажется ObjectPtr для ключа
        ObjectPtr keyObj = pop_value();
        if (!keyObj) throw RuntimeError("Line ...: internal error: null key in dict");
        // 2.2) Затем вычисляем выражение значения
        if (valueExpr) {
            valueExpr->accept(*this);
        } else {
            // Аналогично, если значения нет — внутренний сбой
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: missing value expression in dict literal"
            );
        }
        // На вершине стека теперь ObjectPtr для значения
        ObjectPtr valueObj = pop_value();
        if (!valueObj) throw RuntimeError("Line ...: internal error: null value in dict");

        // 2.3) Пробуем вставить пару (keyObj, valueObj) в dictObj.
        //      В PyDict __setitem__ реализован так, что если ключа ещё нет, он создаётся,
        //      а если ключ уже есть (repr совпадает), значение перезаписывается.
        //      Возможная RuntimeError возникает, если, например, __repr__ у ключа падает
        //      (или другие внутренние ошибки), но чаще всего всё проходит гладко.
        try {
            dictObj->__setitem__(keyObj, valueObj);
        }
        catch (const RuntimeError &err) {
            // Если __setitem__ бросил ошибку, превращаем её в TypeError с указанием строки
            // Формат в духе Python: "Line X TypeError: <текст_ошибки>"
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: " + err.what()
            );
        }
    }

    // 3) После того как все пары вставлены, кладём готовый словарь на стек
    push_value(dictObj);
}


void Executor::visit(SetExpr &node) {
    // В Python литералы множеств выглядят так: {e1, e2, e3, …}.
    // При создании множества:
    //  1) вычисляем каждое подвыражение элемента по порядку,
    //  2) проверяем, что элемент можно использовать как ключ (hashable),
    //     в упрощённой модели — это значит, что он не является list, dict или set,
    //  3) собираем все элементы в промежуточный вектор,
    //  4) создаём объект PySet из этого вектора (конструктор PySet удалит дубли),
    //  5) если на каком-то шаге произошла ошибка, бросаем RuntimeError с указанием строки.

    // 1) Временный вектор для собранных элементов
    std::vector<ObjectPtr> tempElems;
    tempElems.reserve(node.elems.size());

    // 2) Пробегаемся по всем элементам узла AST
    for (size_t i = 0; i < node.elems.size(); ++i) {
        auto &elemExpr = node.elems[i];

        // 2.1) Каждый элемент — это указатель на Expression, проверим, что он не nullptr
        if (!elemExpr) {
            // На практике парсер не создаст nullptr здесь, но на всякий случай:
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: missing element in set literal"
            );
        }

        // 2.2) Вычисляем выражение элемента
        elemExpr->accept(*this);
        // После accept() на вершине стека окажется результат ObjectPtr
        ObjectPtr elemVal = pop_value();

        if (!elemVal) {
        throw RuntimeError("Line " + std::to_string(node.line)
                       + ": internal error: set element evaluated to null");
        }

        // 2.3) Проверяем «hashability» элемента.
        // В нашей модели list, dict и set считать «нехэшируемыми»:
        // отбрасываем их с TypeError.
        if (std::dynamic_pointer_cast<PyList>(elemVal)) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: unhashable type: 'list'"
            );
        }
        if (std::dynamic_pointer_cast<PyDict>(elemVal)) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: unhashable type: 'dict'"
            );
        }
        if (std::dynamic_pointer_cast<PySet>(elemVal)) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: unhashable type: 'set'"
            );
        }

        // 2.4) Если всё в порядке, добавляем элемент во временный вектор
        tempElems.push_back(elemVal);
    }

    // 3) После того как все элементы вычислены и проверены, создаём новый PySet.
    //    Конструктор PySet уберёт дубли по repr().
    auto setObj = std::make_shared<PySet>(std::move(tempElems));

    // 4) Кладём полученное множество на стек значений
    push_value(setObj);
}



void Executor::visit(CondStat &node) {
    // В Python конструкция условного оператора выглядит так:
    // if <condition>:
    //     <ifblock>
    // elif <condition2>:
    //     <elifblock1>
    // elif <condition3>:
    //     <elifblock2>
    // else:
    //     <elseblock>
    //
    // Здесь нужно:
    // 1. Вычислить условие «if».
    // 2. Определить его truthiness (правила Python).
    // 3. Если оно истинно — выполнить if-блок и закончить.
    // 4. Иначе по порядку проверить каждое «elif»: вычисляем выражение, 
    //    проверяем truthiness, если истинно — выполняем соответствующий блок и выходим.
    // 5. Если ни «if», ни один «elif» не сработал, и есть «else» — выполнить else-блок.
    // 6. Если нет «else» — ничего не делать.
    //
    // Если при вычислении какого-то выражения выпадет RuntimeError, он просто пробросится дальше.
    // Если при выполнении блока попадём внутрь других операторов, то они сами себя обрабатывают.
    //
    // Обратите внимание: это оператор, поэтому мы не кладём никаких значений на стек после выполнения блоков.

    // --- Вспомогательная функция для определения truthiness в Python ---

    // --- 1) Обработка блока if ---
    if (!node.condition) {
        // На всякий случай: если нет условия, это внутренняя ошибка AST
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": internal error: missing condition in if-statement"
        );
    }
    // 1.1) Вычисляем выражение условия
    node.condition->accept(*this);
    // Теперь на вершине стека — ObjectPtr с результатом
    ObjectPtr condVal = pop_value();

    // 1.2) Проверяем truthiness
    bool condIsTrue = is_truthy(condVal);

    // 1.3) Если условие истинно — выполняем if-блок
    if (condIsTrue) {
        if (!node.ifblock) {
            // Если вдруг нет блока после if, это внутренняя ошибка AST
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: missing block after if"
            );
        }
        // Выполняем тело if-блока
        node.ifblock->accept(*this);
        // Закончили обработку cond-оператора
        return;
    }

    // --- 2) Если if не сработал, пробегаем по каждому elif в порядке записи ---
    for (size_t idx = 0; idx < node.elifblocks.size(); ++idx) {
        auto &pair = node.elifblocks[idx];
        Expression *elifCond = pair.first.get();
        BlockStat *elifBlock = pair.second.get();

        if (!elifCond) {
            // Если у какой-то пары нет выражения условия, считаем это AST-ошибкой
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: missing condition in elif #" + std::to_string(idx + 1)
            );
        }
        // 2.1) Вычисляем выражение elifCond
        elifCond->accept(*this);
        ObjectPtr elifVal = pop_value();

        // 2.2) Проверяем truthiness
        bool elifIsTrue = is_truthy(elifVal);

        // 2.3) Если истинно — выполняем соответствующий блок и выходим
        if (elifIsTrue) {
            if (!elifBlock) {
                // Если нет блока после этого elif, тоже внутренняя ошибка AST
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + ": internal error: missing block after elif #" + std::to_string(idx + 1)
                );
            }
            elifBlock->accept(*this);
            return;
        }
        // Иначе — идём к следующему elif
    }

    // --- 3) Если ни if, ни один elif не сработали, и есть else — выполняем else-блок ---
    if (node.elseblock) {
        node.elseblock->accept(*this);
    }
    // Если else нет, просто ничего не делаем
}



void Executor::visit(WhileStat &node) {
    // В Python цикл while имеет синтаксис:
    // while <condition>:
    //     <body>
    //
    // Мы должны:
    // 1) Перед каждой итерацией вычислять условие.
    // 2) Проверять его "truthiness" (правила Python: False, 0, 0.0, "", [], {}, set(), None считаются ложью; всё остальное — истина).
    // 3) Если условие истинно — выполнить тело, а затем повторить цикл.
    // 4) Если условие ложно — выйти из цикла и не возвращать никакого значения.
    //
    // Заметим, что здесь не обрабатываются операторы break и continue — если они встретятся внутри тела,
    // то либо они должны быть реализованы отдельно (например, через исключения), либо мы просто игнорируем их.
    // В этой версии реализуем простой цикл "while", без поддержки break/continue.

    // 1) Проверим, что узел condition не пустой (это внутренняя проверка целостности AST).
    if (!node.condition) {
        // Если вдруг AST не соответствует ожиданиям, бросаем RuntimeError.
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": internal error: missing condition in while-statement"
        );
    }

    // Вспомогательный лямбда-функция для проверки «truthiness» в стиле Python.
    // Мы проверяем основные примитивные типы: bool, int, float, string, list, dict, set, None.
    // Для всех остальных объектов считаем, что они истинны.
    

    // 2) Запускаем сам цикл
    while (true) {
        // 2.1) Вычисляем условие node.condition
        node.condition->accept(*this);
        // После accept(...) на стеке valueStack должен оказаться результат условия
        ObjectPtr condVal = pop_value();

        // 2.2) Проверяем «truthiness»
        bool condIsTrue = is_truthy(condVal);

        // 2.3) Если условие ложно – выходим из цикла
        if (!condIsTrue) {
            break;
        }

        // 2.4) Иначе — условие истинно, выполняем тело цикла
        if (node.body) {
        try {
            node.body->accept(*this);
        }
        catch (const BreakException &) {
            break;    // выходим из цикла
        }
        catch (const ContinueException &) {
            continue; // переходим к следующей итерации
        }
        } else {
            break;
        }

        // После выполнения тела мы возвращаемся сюда и начнём следующую итерацию:
        // вычислим условие заново и так далее.
    }

    // Когда мы выйдем из цикла (условие стало ложным), просто завершаем visit.
    // Цикл не возвращает никакого значения в стеке — это управляющая конструкция.
}


void Executor::visit(ForStat &node) {
    // Реализация цикла for в стиле Python:
    // for <var1>[, <var2>, ...] in <iterable>:
    //     <body>
    //
    // Поддерживаем:
    // - Итерацию по спискам (PyList): каждый элемент списка присваивается переменной(ам).
    // - Итерацию по строкам (PyString): каждый символ (как строка длины 1) присваивается переменной.
    // - Распаковку, если в node.iterators несколько имён: ожидаем, что элемент iterable будет PyList той же длины.
    // Для всех остальных типов бросаем TypeError: "<type> object is not iterable".

    // 1) Подготовка: убедимся, что переменные-итераторы существуют в текущем (локальном) скоупе.
    //    Если какой-либо переменной пока нет, создаём её в локальном скоупе с nullptr-значением.
    for (const auto &varName : node.iterators) {
        if (!scopes.lookup_local(varName)) {
            Symbol sym;
            sym.name = varName;
            sym.type = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl = &node;  // для связывания с этим AST-узлом
            scopes.insert(sym);
        }
    }

    // 2) Вычислим выражение iterable.
    if (!node.iterable) {
        throw RuntimeError(
            "Line " + std::to_string(node.line) 
            + ": internal error: missing iterable in for-statement"
        );
    }
    node.iterable->accept(*this);
    ObjectPtr iterableVal = pop_value();

    if (!iterableVal) {
        throw RuntimeError("Line ...: internal error: iterable is null");
    }

    // Вспомогательная лямбда для получения строкового представления типа объекта,
    // чтобы корректно формировать сообщения об ошибках.
    

    // 3) Реализуем поведение для списков (PyList).
    if (auto listObj = std::dynamic_pointer_cast<PyList>(iterableVal)) {
        const auto &elements = listObj->getElements();
        // Проходим по каждому элементу списка
        for (size_t idx = 0; idx < elements.size(); ++idx) {
            ObjectPtr element = elements[idx];

            // 3.1) Если у нас единичный итератор, просто связываем его с element.
            if (node.iterators.size() == 1) {
                Symbol *sym = scopes.lookup_local(node.iterators[0]);
                sym->value = element;
            }
            // 3.2) Если у нас несколько имён-итераторов (распаковка), ожидаем, что элемент тоже PyList
            else {
                auto innerList = std::dynamic_pointer_cast<PyList>(element);
                if (!innerList) {
                    throw RuntimeError(
                        "Line " + std::to_string(node.line)
                        + " TypeError: cannot unpack non-iterable element '" 
                        + element->repr() + "'"
                    );
                }
                const auto &innerElems = innerList->getElements();
                if (innerElems.size() != node.iterators.size()) {
                    throw RuntimeError(
                        "Line " + std::to_string(node.line)
                        + " ValueError: not enough values to unpack (expected "
                        + std::to_string(node.iterators.size()) + ", got "
                        + std::to_string(innerElems.size()) + ")"
                    );
                }
                // Присваиваем по позициям
                for (size_t k = 0; k < node.iterators.size(); ++k) {
                    Symbol *sym = scopes.lookup_local(node.iterators[k]);
                    sym->value = innerElems[k];
                }
            }

            // 3.3) Выполняем тело цикла
            if (node.body) {
                try {
                    node.body->accept(*this);
                }
                catch (const BreakException &) {
                    break;
                }
                catch (const ContinueException &) {
                    continue;
                }
            }
        }
    return;
}

    // 4) Реализуем поведение для строк (PyString) — итерируем по символам.
    if (auto strObj = std::dynamic_pointer_cast<PyString>(iterableVal)) {
        const std::string &s = strObj->get();
        for (size_t i = 0; i < s.size(); ++i) {
            // Каждый символ представляем как PyString длины 1
            auto charObj = std::make_shared<PyString>(std::string(1, s[i]));

            // 4.1) Если один итератор, присваиваем символ
            if (node.iterators.size() == 1) {
                Symbol *sym = scopes.lookup_local(node.iterators[0]);
                sym->value = charObj;
            }
            // 4.2) Если множественная распаковка — это не поддерживается для строк в Python стандартно,
            //       поэтому бросаем ошибку распаковки.
            else {
                throw RuntimeError(
                    "Line " + std::to_string(node.line)
                    + " TypeError: cannot unpack non-iterable element '" 
                    + charObj->repr() + "'"
                );
            }

            // 4.3) Выполняем тело цикла
            if (node.body) {
                try {
                    node.body->accept(*this);
                }
                catch (const BreakException &) {
                    break;
                }
                catch (const ContinueException &) {
                    continue;
                }
            }
        }
        return;
    }

    // 5) Для любых других типов (dict, set, пользовательские объекты и т.д.) — 
    //    бросаем TypeError: '<type>' object is not iterable.
    std::string badType = deduceTypeName(iterableVal);
    throw RuntimeError(
        "Line " + std::to_string(node.line)
        + " TypeError: '" + badType + "' object is not iterable"
    );
}

void Executor::visit(BreakStat &node) {
    // В Python break может быть только внутри цикла.
    // Здесь мы просто выбрасываем BreakException, которую
    // должны поймать вокруг тела цикла (в visit(ForStat) или visit(WhileStat)).
    //
    // Если break встретится вне цикла, это исключение поднимется
    // до самого execute(), и превратится в необработанную ошибку времени выполнения,
    // что соответствует поведению «break outside loop».
    throw BreakException{};
}

void Executor::visit(ContinueStat &node) {
    // Аналогично break, continue выбрасывает своё исключение.
    // Цикл (ForStat или WhileStat) должен его поймать и перейти к следующей итерации.
    throw ContinueException{};
}

void Executor::visit(PassStat &node) {
    // Оператор pass в Python ничего не делает — просто «пустая» инструкция.
    // Здесь не нужно вычислять выражения и не нужно менять состояние.
    // Поэтому оставляем этот метод пустым.
}

void Executor::visit(AssertStat &node) {
    // В Python выражение assert проверяет условие, и если оно ложно, вызывает AssertionError.
    // Синтаксис: assert <condition>, <optional_message>
    //
    // 1) Сначала вычисляем условие:
    if (!node.condition) {
        // На всякий случай проверяем, вдруг узел пустой — это внутренняя ошибка
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": internal error: AssertStat has no condition"
        );
    }
    node.condition->accept(*this);
    ObjectPtr condObj = pop_value();

    // 2) Определяем truthiness так же, как в других местах:

    bool condTrue = is_truthy(condObj);
    if (condTrue) {
        // Условие истинно — assert ничего не делает
        return;
    }

    // 3) Если условие ложно — нужно бросить AssertionError.
    // Если был передан второй аргумент (сообщение), вычисляем его:
    std::string message;
    if (node.message) {
        // node.expr — это сообщение, которое передается после запятой
        node.message->accept(*this);
        ObjectPtr msgObj = pop_value();
        // Делаем repr() и используем это в сообщении
        message = msgObj->repr();
    }

    // 4) Формируем текст ошибки. В Python бывает просто "AssertionError",
    // или "AssertionError: <message>".
    std::string errorText = "AssertionError";
    if (!message.empty()) {
        errorText += ": ";
        errorText += message;
    }

    // 5) Бросаем RuntimeError с текстом AssertionError
    throw RuntimeError(
        "Line " + std::to_string(node.line) + " " + errorText
    );
}

void Executor::visit(ExitStat &node) {
    // В Python вызов quit()/exit() (или sys.exit()) вызывает SystemExit.
    // Мы эмулируем поведение: если аргумент — целое число, используем его как код возврата;
    // если это любой другой объект, печатаем его и возвращаем код 1.
    //
    // 1) Если есть выражение, вычисляем его; иначе считаем, что exit() без аргументов → код 0.
    int exitCode = 0;
    if (node.expr) {
        node.expr->accept(*this);
        ObjectPtr val = pop_value();

        // Если это PyInt или PyBool или PyFloat, пытаемся получить целое:
        if (auto i = std::dynamic_pointer_cast<PyInt>(val)) {
            exitCode = i->get();
        }
        else if (auto b = std::dynamic_pointer_cast<PyBool>(val)) {
            // True → 1, False → 0
            exitCode = b->get() ? 1 : 0;
        }
        else if (auto f = std::dynamic_pointer_cast<PyFloat>(val)) {
            // Приводим к int (отбрасываем дробную часть)
            exitCode = static_cast<int>(f->get());
        }
        else {
            // Любой другой объект: печатаем его repr() как текст на stderr и выходим с кодом 1
            std::cerr << val->repr() << std::endl;
            std::exit(1);
        }
    }

    // 2) Теперь действительно выходим с собранным кодом
    std::exit(exitCode);
}

void Executor::visit(PrintStat &node) {
    // В Python print <expr> выводит repr(expr) и перевод строки.
    // Здесь мы вычисляем expr и печатаем результат, если он есть.

    if (!node.expr) {
        // Если вдруг нет выражения — это просто печатаем пустую строку
        std::cout << std::endl;
        return;
    }

    // 1) Вычисляем выражение
    node.expr->accept(*this);
    ObjectPtr val = pop_value();

    // 2) Печатаем repr() объекта и переводим строку
    std::cout << val->repr() << std::endl;
}

void Executor::visit(LenStat &node) {}
void Executor::visit(DirStat &node) {}
void Executor::visit(EnumerateStat &node) {}


void Executor::visit(ClassDecl &node) {

    std::shared_ptr<PyClass> parentClass = nullptr;

    // Если в AST указано хоть одно имя базового класса:
    if (!node.baseClasses.empty()) {
        // 1.1) Если указано более одного — мы не поддерживаем множественное наследование
        if (node.baseClasses.size() > 1) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": multiple inheritance is not supported"
            );
        }

        // 1.2) Берём единственное имя базового класса
        const std::string &baseName = node.baseClasses[0];

        // 1.3) Ищем его в текущих областях видимости
        Symbol *baseSym = scopes.lookup(baseName);
        if (!baseSym) {
            // Класс-наследник ссылается на несуществующее имя
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": name '" + baseName + "' is not defined"
            );
        }

        // 1.4) Проверяем, что найденный символ действительно представляет собой класс
        //      То есть baseSym->value должно быть PyClass
        ObjectPtr baseObj = baseSym->value;
        parentClass = std::dynamic_pointer_cast<PyClass>(baseObj);
        if (!parentClass) {
            // Если это не PyClass, бросаем TypeError
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": TypeError: '" + baseName + "' is not a class"
            );
        }
    }

    // 1) Сначала создаём «объект класса» PyClass, у которого будет своё собственное пространство имён.
    //    Передаём ему строку с именем класса node.name.
    auto classObj = std::make_shared<PyClass>(node.name, parentClass);

    // 2) Регистрируем сам класс в текущей (лексической) таблице символов,
    //    чтобы сразу после объявления можно было писать:
    //        class X: ...
    //        x = X()
    //    и имя X было доступно. 
    Symbol classSymbol;
    classSymbol.name  = node.name;
    classSymbol.type  = SymbolType::UserClass;
    classSymbol.value = classObj;
    classSymbol.decl  = &node;

    // Если в локальном скоупе уже есть символ с таким именем – перезапишем, иначе – просто вставим.
    if (auto existing = scopes.lookup_local(node.name)) {
        *existing = classSymbol;
    } else {
        scopes.insert(classSymbol);
    }

    // 3) Обрабатываем все «поля» (FieldDecl) внутри тела класса. 
    //    Для каждого FieldDecl: 
    //       a) вычисляем выражение-инициализатор (если оно есть),
    //       b) кладём это значение в словарь класса classObj->classDict под ключом fieldName.
    //    Если нет инициализатора, по аналогии с Python ставим None.

    for (auto &fieldPtr : node.fields) {
        const std::string &fieldName = fieldPtr->name; 
        ObjectPtr initValue;

        if (!initValue) initValue = std::make_shared<PyNone>();

        // 3.1) Если у поля есть initExpr, то вычисляем его и «снимаем» результат со стека.
        if (fieldPtr->initExpr) {
            fieldPtr->initExpr->accept(*this);
            initValue = pop_value();
        } else {
            // Если инициализатора нет вовсе – кладём PyNone
            initValue = std::make_shared<PyNone>();
        }

        // 3.2) Собираем ключ (PyString с именем поля) и кладём пару (ключ, значение) в classDict
        auto key = std::make_shared<PyString>(fieldName);
        classObj->classDict->__setitem__(key, initValue);
    }

    // 4) Обрабатываем все «методы» (FuncDecl) внутри тела класса. 
    //    Для каждого метода нам надо:
    //      4.1) вычислить все default-значения аргументов (если они есть),
    //      4.2) сконструировать PyFunction с full-аргументами (имя, указатель на FuncDecl,
    //            лексическое окружение, список posParams и вектор defaultValues),
    //      4.3) поместить полученный объект в classObj->classDict под ключом methodName,
    //      чтобы позднее при вызове через <экземпляр>.<method> работало корректно.

    for (auto &methodPtr : node.methods) {
        // Получаем «сырой» указатель на AST-узел FuncDecl
        FuncDecl *fdecl = methodPtr.get();

        // ------------------------
        // 4.1) Собираем default-значения для каждого параметра метода.
        //      AST-узел FuncDecl::defaultParams хранит пары (имя, Expression*).
        //      Мы должны «пройтись» по этому вектору, вычислить каждое выражение и
        //      положить результат в вектор default_values.
        // ------------------------
        std::vector<ObjectPtr> default_values;
        default_values.reserve(fdecl->defaultParams.size());

        for (auto &pr : fdecl->defaultParams) {
            // pr.first  – имя параметра (string)
            // pr.second – unique_ptr<Expression> или nullptr, если default явно не задан
            Expression *exprNode = pr.second.get();

            if (exprNode) {
                // Вычисляем выражение default‐параметра прямо сейчас
                exprNode->accept(*this);
                ObjectPtr val = pop_value();
                default_values.push_back(val);
            } else {
                // Если вдруг в defaultParams попал nullptr (маловероятно), ставим None
                default_values.push_back(std::make_shared<PyNone>());
            }
        }

        // ------------------------
        // 4.2) Сконструируем объект PyFunction.
        //       Ему нужно передать:
        //         - имя функции: fdecl->name
        //         - указатель на AST-узел FuncDecl: fdecl
        //         - лексическое окружение (таблица символов) – прямо currentTable()
        //         - список positional параметров – fdecl->posParams
        //         - вектор заранее вычисленных default‐значений – default_values
        // ------------------------
        auto fnObj = std::make_shared<PyFunction>(
            fdecl->name,               // const std::string &funcName
            fdecl,                     // FuncDecl *f
            scopes.currentTable(),     // std::shared_ptr<SymbolTable> enclosingEnv
            fdecl->posParams,          // const std::vector<std::string> &parameters
            std::move(default_values)  // std::vector<ObjectPtr> defaults
        );

        // ------------------------
        // 4.3) Кладём fnObj в словарь класса classDict под ключом methodName.
        //       После этого, при запросе myInstance.methodName, в __getattr__ сначала найдётся
        //       метод в classDict, и дальше его можно будет вызывать как обычную функцию.
        // ------------------------
        auto key = std::make_shared<PyString>(fdecl->name);
        classObj->classDict->__setitem__(key, fnObj);
    }

    // 5) В конце кладём сам объект класса (classObj) на «стек значений».
    //    Это нужно, чтобы если кто-то написал что-то вроде:
    //         C = class Foo: ... 
    //     и с точки зрения AST это выражение возвращает сам объект класса,
    //     т. е. «результатом» Visit(ClassDecl) должно быть classObj.
    push_value(classObj);
}


void Executor::visit(ListComp &node) {
    // Шаг 1: вычисляем «итерируемый» объект
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();
    if (!iterableVal) {
        throw RuntimeError(
            "Line " + std::to_string(node.line) 
            + ": internal error: iterable evaluated to null"
        );
    }

    // Шаг 2: выделяем вектор «сырых» элементов, по которым будем итерироваться.
    // Поддерживаем PyList и PyString. Всё остальное — не итерируемый.
    std::vector<ObjectPtr> rawElems;
    if (auto listObj = std::dynamic_pointer_cast<PyList>(iterableVal)) {
        rawElems = listObj->getElements();
    }
    else if (auto strObj = std::dynamic_pointer_cast<PyString>(iterableVal)) {
        const std::string &s = strObj->get();
        rawElems.reserve(s.size());
        for (char c : s) {
            rawElems.push_back(std::make_shared<PyString>(std::string(1, c)));
        }
    }
    else {
        // Пробуем вызвать __iter__/__getitem__, но для простоты бросим TypeError:
        std::string tname = deduceTypeName(iterableVal);
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: '" + tname + "' object is not iterable"
        );
    }

    // Шаг 3: для каждого элемента «итерируемого»:
    //  a) создаём символ в локальном scope с именем node.iterVar → присваиваем текущий элемент
    //  b) вычисляем valueExpr (там может быть обращение к iterVar)
    //  c) сохраняем результат в вектор resultElems
    std::vector<ObjectPtr> resultElems;
    resultElems.reserve(rawElems.size());

    for (size_t i = 0; i < rawElems.size(); ++i) {
        ObjectPtr element = rawElems[i];

        // 3.a) Если имя iterVar ещё не было объявлено в локальном scope, мы вставляем его:
        if (!scopes.lookup_local(node.iterVar)) {
            Symbol sym;
            sym.name  = node.iterVar;
            sym.type  = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl  = nullptr; // было бы nice указывать на node, но не критично
            scopes.insert(sym);
        }
        // Кладём в локальный scope текущее значение element:
        Symbol *iterSym = scopes.lookup_local(node.iterVar);
        iterSym->value = element;

        // 3.b) Теперь вычисляем valueExpr:
        node.valueExpr->accept(*this);
        ObjectPtr val = pop_value();
        if (!val) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: list comprehension element evaluated to null"
            );
        }
        resultElems.push_back(val);
    }

    // Шаг 4: после всех итераций собираем PyList из resultElems
    auto listObj = std::make_shared<PyList>(std::move(resultElems));
    push_value(listObj);
}


void Executor::visit(DictComp &node) {
    // Шаг 1: вычисляем iterable
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();
    if (!iterableVal) {
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": internal error: iterable evaluated to null"
        );
    }

    // Шаг 2: собираем сырой вектор элементов
    std::vector<ObjectPtr> rawElems;
    if (auto listObj = std::dynamic_pointer_cast<PyList>(iterableVal)) {
        rawElems = listObj->getElements();
    }
    else if (auto strObj = std::dynamic_pointer_cast<PyString>(iterableVal)) {
        const std::string &s = strObj->get();
        rawElems.reserve(s.size());
        for (char c : s) {
            rawElems.push_back(std::make_shared<PyString>(std::string(1, c)));
        }
    }
    else {
        std::string tname = deduceTypeName(iterableVal);
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: '" + tname + "' object is not iterable"
        );
    }

    // Шаг 3: создаём новый пустой словарь, потом наполняем его
    auto dictObj = std::make_shared<PyDict>();

    for (size_t i = 0; i < rawElems.size(); ++i) {
        ObjectPtr element = rawElems[i];

        // 3.a) «вставляем» в локальный scope iterVar = element
        if (!scopes.lookup_local(node.iterVar)) {
            Symbol sym;
            sym.name  = node.iterVar;
            sym.type  = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl  = nullptr;
            scopes.insert(sym);
        }
        Symbol *iterSym = scopes.lookup_local(node.iterVar);
        iterSym->value = element;

        // 3.b) вычисляем keyExpr
        node.keyExpr->accept(*this);
        ObjectPtr keyObj = pop_value();
        if (!keyObj) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: dict comprehension key evaluated to null"
            );
        }

        // 3.c) вычисляем valueExpr
        node.valueExpr->accept(*this);
        ObjectPtr valueObj = pop_value();
        if (!valueObj) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: dict comprehension value evaluated to null"
            );
        }

        // 3.d) вставляем в dictObj
        try {
            dictObj->__setitem__(keyObj, valueObj);
        }
        catch (const RuntimeError &err) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + " TypeError: " + err.what()
            );
        }
    }

    push_value(dictObj);
}

void Executor::visit(TupleComp &node) {
    // Почти точь-в-точь как в ListComp
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();
    if (!iterableVal) {
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + ": internal error: iterable evaluated to null"
        );
    }

    std::vector<ObjectPtr> rawElems;
    if (auto listObj = std::dynamic_pointer_cast<PyList>(iterableVal)) {
        rawElems = listObj->getElements();
    }
    else if (auto strObj = std::dynamic_pointer_cast<PyString>(iterableVal)) {
        const std::string &s = strObj->get();
        rawElems.reserve(s.size());
        for (char c : s) {
            rawElems.push_back(std::make_shared<PyString>(std::string(1, c)));
        }
    }
    else {
        std::string tname = deduceTypeName(iterableVal);
        throw RuntimeError(
            "Line " + std::to_string(node.line)
            + " TypeError: '" + tname + "' object is not iterable"
        );
    }

    std::vector<ObjectPtr> resultElems;
    resultElems.reserve(rawElems.size());

    for (size_t i = 0; i < rawElems.size(); ++i) {
        ObjectPtr element = rawElems[i];
        if (!scopes.lookup_local(node.iterVar)) {
            Symbol sym;
            sym.name  = node.iterVar;
            sym.type  = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl  = nullptr;
            scopes.insert(sym);
        }
        Symbol *iterSym = scopes.lookup_local(node.iterVar);
        iterSym->value = element;

        node.valueExpr->accept(*this);
        ObjectPtr val = pop_value();
        if (!val) {
            throw RuntimeError(
                "Line " + std::to_string(node.line)
                + ": internal error: tuple comprehension element evaluated to null"
            );
        }
        resultElems.push_back(val);
    }

    // вместо PyTuple просто «отдаём» PyList
    auto listObj = std::make_shared<PyList>(std::move(resultElems));
    push_value(listObj);
}


// ------------------------------------
// visit(LambdaExpr):
// Когда мы видим «lambda x,y: <expr>», мы хотим вернуть
// объект типа PyFunction, который при вызове посчитает <expr>.
// ------------------------------------
void Executor::visit(LambdaExpr &node) {
    // 1) Сразу «вырвем» из LambdaExpr само выражение тела (move),
    //    чтобы не вычислять его на этом этапе, а передать в новый FuncDecl.
    //    После этого node.body станет nullptr (мы больше к нему не обратимся —
    //    лямбда создаётся только один раз).
    std::unique_ptr<Expression> bodyExpr = std::move(node.body);

    // 2) Оборачиваем тело лямбды в одну-единственную инструкцию ReturnStat,
    //    иначе PyFunction не «увидит» какое значение возвращать.
    //    ReturnStat сам бросит ReturnException, когда встретится во время исполнения.
    auto returnStmt = std::make_unique<ReturnStat>(std::move(bodyExpr), node.line);

    // 3) Создадим «временный» FuncDecl. Назовём его "<lambda>", 
    //    перенесём туда список параметров (node.params) и наш returnStmt.
    //    (По факту мы роняем все default-параметры, они у лямбды не поддерживаются.)
    FuncDecl *lambdaDecl = new FuncDecl(
        "<lambda>",                     // имя функции (строго говоря, оно не используется)
        node.params,                    // параметры (имена)
        std::vector<std::pair<std::string, std::unique_ptr<Expression>>>{}, // default-параметры пустые
        std::move(returnStmt),          // тело — это единственный ReturnStat
        node.line                       // строка, где встретилась лямбда
    );

    // 4) Создаём объект PyFunction, передавая ему:
    //    - имя "<lambda>"
    //    - указатель на lambdaDecl (AST-узел)
    //    - текущее лексическое окружение (то, что лежит в scopes.currentTable())
    //    - вектор имён позиционных параметров
    //    - пустой вектор default-значений (их у нас нет)
    auto lambdaObj = std::make_shared<PyFunction>(
        "<lambda>",
        lambdaDecl,
        scopes.currentTable(),
        node.params,
        std::vector<ObjectPtr>{}     // default-значения (пустой список)
    );

    // 5) Кладём получившийся объект PyFunction на стек значений.
    //    Когда кто-нибудь вызовет этот объект, в нём запустится
    //    лексическое окружение, и выполнятся наши ReturnStat-ы.
    push_value(lambdaObj);
}