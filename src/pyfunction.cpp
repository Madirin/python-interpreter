#include "object.hpp"     // здесь объявлены PyFunction, ObjectPtr, RuntimeError, Symbol, и т.д.
#include "executer.hpp"   // здесь объявлен класс Executor, ReturnException, SymbolType, Symbol и т.д.

// Нам также понадобятся эти два include для std::string и std::to_string:
#include <string>
#include <vector>


std::type_index PyFunction::type() const {
    return typeid(PyFunction);
}

std::string PyFunction::repr() const {
    std::ostringstream ss;
    ss << "<function " << decl->name << " at " << this << ">";
    return ss.str();
}

FuncDecl* PyFunction::getDecl() const {
    return decl;
}


PyFunction::PyFunction(const std::string &funcName,
               FuncDecl *f,
               std::shared_ptr<SymbolTable> scope,
               const std::vector<std::string> &parameters,
               std::vector<ObjectPtr> defaults)
        : name(funcName),
          decl(f),
          scope(std::move(scope)),
          posParams(parameters),
          defaultValues(std::move(defaults))
    {}

ObjectPtr PyFunction::__call__(const std::vector<ObjectPtr> &args) {
    // Мы собираемся выполнить тело пользовательской функции, используя Executor.
    // Идея проста:
    // 1. Создаем новый Executor.
    // 2. Создаем в нем «локальную область» (enter_scope), чтобы в ней лежали параметры функции,
    //    а в родительской (глобальной) остались встроенные переменные (print, range и т. д.).
    // 3. Привязываем позиционные и default-параметры к именам в этой новой локальной области.
    // 4. Запускаем тело функции (decl->body) через accept(exec). Если встретится Return, то
    //    поймаем ReturnException и извлечем возвращаемое значение.
    // 5. Уходим из локальной области (leave_scope) и возвращаем полученное значение.

    // Шаг 1: создаем Executor. Его конструктор уже заведет встроенные функции (print, range...)
    Executor exec;

    // Шаг 2: заводим новый промежуточный scope для локальных переменных этой функции.
    //         Родительским останется глобальный (с встроенными).
    exec.scopes.enter_scope();

    // Шаг 3a: привязываем все позиционные параметры (decl->posParams) к переданным аргументам.
    const auto &paramNames = decl->posParams;
    size_t requiredCount = paramNames.size();
    if (args.size() < requiredCount) {
        // Если передали меньше, чем обязательных позиционных – бросаем исключение напрямую.
        throw RuntimeError(
            decl->name + "() missing required positional arguments"
        );
    }
    for (size_t i = 0; i < requiredCount; ++i) {
        Symbol s;
        s.name  = paramNames[i];
        s.type  = SymbolType::Parameter;
        s.value = args[i];
        // Вставка в локальную таблицу нового scope — раз вернулись после enter_scope().
        exec.scopes.insert(s);
    }

    // Шаг 3b: обрабатываем default-параметры. У нас decl->defaultParams – вектор пар {имя, Expression*}.
    //         Если аргумент для этого параметра передан (args.size() > requiredCount + idx),
    //         мы берем из args, иначе – берем заранее сохраненное defaultValue из this->defaultValues.
    const auto &declDefParams = decl->defaultParams;           // из AST: пары (имя, узел Expression*)
    const auto &storedDefaults = this->defaultValues;          // из PyFunction: уже вычисленные объекты

    for (size_t i = 0; i < declDefParams.size(); ++i) {
        const std::string &paramName = declDefParams[i].first;
        ObjectPtr valueToBind;

        size_t argIndex = requiredCount + i;
        if (argIndex < args.size()) {
            // Если в args есть «перекрывающий» позиционный аргумент
            valueToBind = args[argIndex];
        } else {
            // Иначе берем заранее вычисленный default из PyFunction
            valueToBind = storedDefaults[i];
        }

        Symbol s;
        s.name  = paramName;
        s.type  = SymbolType::Parameter;
        s.value = valueToBind;
        exec.scopes.insert(s);
    }

    // Если передали слишком много аргументов (больше, чем posParams + defaultParams), бросим ошибку:
    size_t maxAllowed = paramNames.size() + declDefParams.size();
    if (args.size() > maxAllowed) {
        throw RuntimeError(
            std::string("TypeError: ") + decl->name + "() takes from "
            + std::to_string(paramNames.size()) + " to "
            + std::to_string(maxAllowed)
            + " positional arguments but "
            + std::to_string(args.size())
            + " were given"
        );
    }

    // Шаг 4: выполняем тело функции. Если внутри встречается return, он кинет ReturnException.
    ObjectPtr returnValue = std::make_shared<PyNone>(); // по умолчанию вернем None
    try {
        if (decl->body) {
            // Запускаем обход дерева через Executor (вызовет все вложенные visit-методы).
            decl->body->accept(exec);
            // Если тело выполнено без return, то оставляем returnValue = None.
        }
    }
    catch (const ReturnException &ret) {
        // Когда где-то внутри visit(ReturnStat) бросили ReturnException, в ret.value лежит нужный ObjectPtr.
        returnValue = ret.value;
    }

    // Шаг 5: выходим из локальной области видимости функции.
    exec.scopes.leave_scope();

    // Возвращаем полученный результат (либо тот, что бросили, либо None)
    return returnValue;
}