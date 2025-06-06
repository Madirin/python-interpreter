#include "printer.hpp"
#include <sstream>
#include <variant>

ASTPrinterVisitor::ASTPrinterVisitor()
    : result("")
    , indentLevel(0)
{}

std::string ASTPrinterVisitor::getResult() const {
    return result;
}

std::string ASTPrinterVisitor::indent() const {
    // Каждый уровень = 2 пробела
    return std::string(indentLevel * 2, ' ');
}

void ASTPrinterVisitor::append(const std::string &str) {
    result += str;
}

// ------------------------------------
// visit(TransUnit):
// Печатает всю единицу перевода: список «функции/классы/операторы»
// ------------------------------------
void ASTPrinterVisitor::visit(TransUnit &node) {
    // Показываем, что это TransUnit, можем упомянуть номер строки (node.line), если нужно.
    append("TransUnit (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // Перебираем все внешние элементы: это либо FuncDecl, ClassDecl, либо Statement
    for (auto &unit : node.units) {
        // unit – это unique_ptr<ASTNode>, мы вызываем accept, и будет диспатч на нужный visit(...)
        unit->accept(*this);
    }

    indentLevel--;
}

// ------------------------------------
// visit(FuncDecl):
// Печатает определение функции, её параметры и тело
// ------------------------------------
void ASTPrinterVisitor::visit(FuncDecl &node) {
    // Строка с описанием «FuncDecl: имя (строка с def)»
    append(indent() + "FuncDecl (line = " + std::to_string(node.line) + "): " + node.name + "\n");
    indentLevel++;

    // Позиционные параметры
    if (!node.posParams.empty()) {
        append(indent() + "Positional parameters:\n");
        indentLevel++;
        for (const auto &paramName : node.posParams) {
            append(indent() + paramName + "\n");
        }
        indentLevel--;
    }

    // Default-параметры
    if (!node.defaultParams.empty()) {
        append(indent() + "Default parameters:\n");
        indentLevel++;
        for (const auto &pr : node.defaultParams) {
            // pr.first – имя параметра, pr.second – Expression*
            append(indent() + pr.first + " =\n");
            indentLevel++;
            pr.second->accept(*this);
            indentLevel--;
        }
        indentLevel--;
    }

    // Тело функции (BlockStat)
    append(indent() + "Body:\n");
    indentLevel++;
    if (node.body) {
        node.body->accept(*this);
    }
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(BlockStat):
// Печатает вложенный блок операторов
// ------------------------------------
void ASTPrinterVisitor::visit(BlockStat &node) {
    append(indent() + "BlockStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    for (auto &stmt : node.statements) {
        stmt->accept(*this);
    }
    indentLevel--;
}

// ------------------------------------
// visit(ExprStat):
// Оператор-выражение
// ------------------------------------
void ASTPrinterVisitor::visit(ExprStat &node) {
    append(indent() + "ExprStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    if (node.expr) {
        node.expr->accept(*this);
    }
    indentLevel--;
}

// ------------------------------------
// visit(CondStat):
// Оператор if-elif-else
// ------------------------------------
void ASTPrinterVisitor::visit(CondStat &node) {
    append(indent() + "CondStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // if-condition
    append(indent() + "If condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;

    // if-block
    append(indent() + "If block:\n");
    indentLevel++;
    node.ifblock->accept(*this);
    indentLevel--;

    // elif-блоки
    for (size_t i = 0; i < node.elifblocks.size(); ++i) {
        append(indent() + "Elif #" + std::to_string(i+1) + " condition:\n");
        indentLevel++;
        node.elifblocks[i].first->accept(*this);
        indentLevel--;

        append(indent() + "Elif #" + std::to_string(i+1) + " block:\n");
        indentLevel++;
        node.elifblocks[i].second->accept(*this);
        indentLevel--;
    }

    // else-блок
    if (node.elseblock) {
        append(indent() + "Else block:\n");
        indentLevel++;
        node.elseblock->accept(*this);
        indentLevel--;
    }

    indentLevel--;
}

// ------------------------------------
// visit(WhileStat):
// Оператор while
// ------------------------------------
void ASTPrinterVisitor::visit(WhileStat &node) {
    append(indent() + "WhileStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    append(indent() + "Condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;

    append(indent() + "Body:\n");
    indentLevel++;
    node.body->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(ForStat):
// Оператор for
// ------------------------------------
void ASTPrinterVisitor::visit(ForStat &node) {
    // Сначала печатаем имена итераторов
    std::string iters = "";
    for (size_t i = 0; i < node.iterators.size(); ++i) {
        iters += node.iterators[i];
        if (i + 1 < node.iterators.size()) {
            iters += ", ";
        }
    }
    append(indent() + "ForStat (line = " + std::to_string(node.line) + "): iterators = [" + iters + "]\n");
    indentLevel++;

    // Iterable
    append(indent() + "Iterable:\n");
    indentLevel++;
    node.iterable->accept(*this);
    indentLevel--;

    // Body
    append(indent() + "Body:\n");
    indentLevel++;
    node.body->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(ReturnStat):
// Оператор return
// ------------------------------------
void ASTPrinterVisitor::visit(ReturnStat &node) {
    append(indent() + "ReturnStat (line = " + std::to_string(node.line) + ")\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}

// ------------------------------------
// visit(BreakStat):
// Оператор break
// ------------------------------------
void ASTPrinterVisitor::visit(BreakStat &node) {
    append(indent() + "BreakStat (line = " + std::to_string(node.line) + ")\n");
}

// ------------------------------------
// visit(ContinueStat):
// Оператор continue
// ------------------------------------
void ASTPrinterVisitor::visit(ContinueStat &node) {
    append(indent() + "ContinueStat (line = " + std::to_string(node.line) + ")\n");
}

// ------------------------------------
// visit(PassStat):
// Оператор pass
// ------------------------------------
void ASTPrinterVisitor::visit(PassStat &node) {
    append(indent() + "PassStat (line = " + std::to_string(node.line) + ")\n");
}

// ------------------------------------
// visit(AssertStat):
// Оператор assert
// ------------------------------------
void ASTPrinterVisitor::visit(AssertStat &node) {
    append(indent() + "AssertStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    if (node.condition) {
        append(indent() + "Condition:\n");
        indentLevel++;
        node.condition->accept(*this);
        indentLevel--;
    }
    if (node.message) {
        append(indent() + "Message:\n");
        indentLevel++;
        node.message->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}

// ------------------------------------
// visit(ExitStat):
// Оператор exit
// ------------------------------------
void ASTPrinterVisitor::visit(ExitStat &node) {
    append(indent() + "ExitStat (line = " + std::to_string(node.line) + "):\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}

// ------------------------------------
// visit(PrintStat):
// Оператор print
// ------------------------------------
void ASTPrinterVisitor::visit(PrintStat &node) {
    append(indent() + "PrintStat (line = " + std::to_string(node.line) + "):\n");
    if (node.expr) {
        indentLevel++;
        node.expr->accept(*this);
        indentLevel--;
    }
}

// ------------------------------------
// visit(AssignStat):
// Оператор присваивания
// ------------------------------------
void ASTPrinterVisitor::visit(AssignStat &node) {
    append(indent() + "AssignStat (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // Левая часть
    append(indent() + "Left:\n");
    indentLevel++;
    if (node.left) {
        node.left->accept(*this);
    }
    indentLevel--;

    // Правая часть
    append(indent() + "Right:\n");
    indentLevel++;
    if (node.right) {
        node.right->accept(*this);
    }
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(UnaryExpr):
// Унарное выражение
// ------------------------------------
void ASTPrinterVisitor::visit(UnaryExpr &node) {
    append(indent() + "UnaryExpr (op = '" + node.op + "', line = " + std::to_string(node.line) + ")\n");
    indentLevel++;
    node.operand->accept(*this);
    indentLevel--;
}

// ------------------------------------
// visit(BinaryExpr):
// Бинарное выражение
// ------------------------------------
void ASTPrinterVisitor::visit(BinaryExpr &node) {
    append(indent() + "BinaryExpr (op = '" + node.op + "', line = " + std::to_string(node.line) + ")\n");
    indentLevel++;

    append(indent() + "Left:\n");
    indentLevel++;
    node.left->accept(*this);
    indentLevel--;

    append(indent() + "Right:\n");
    indentLevel++;
    node.right->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(PrimaryExpr):
// Самая «первичная» форма: литерал, идентификатор, вызов, индекс, скобки, тернар
// ------------------------------------
void ASTPrinterVisitor::visit(PrimaryExpr &node) {
    // Сначала скажем, какой именно вариант PrimaryType
    append(indent() + "PrimaryExpr (");
    switch (node.type) {
        case PrimaryExpr::PrimaryType::LITERAL:
            append("LITERAL, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.literalExpr) node.literalExpr->accept(*this);
            indentLevel--;
            break;
        case PrimaryExpr::PrimaryType::ID:
            append("ID, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.idExpr) node.idExpr->accept(*this);
            indentLevel--;
            break;
        case PrimaryExpr::PrimaryType::CALL:
            append("CALL, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.callExpr) node.callExpr->accept(*this);
            indentLevel--;
            break;
        case PrimaryExpr::PrimaryType::INDEX:
            append("INDEX, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.indexExpr) node.indexExpr->accept(*this);
            indentLevel--;
            break;
        case PrimaryExpr::PrimaryType::PAREN:
            append("PAREN, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.parenExpr) node.parenExpr->accept(*this);
            indentLevel--;
            break;
        case PrimaryExpr::PrimaryType::TERNARY:
            append("TERNARY, line = " + std::to_string(node.line) + ")\n");
            indentLevel++;
            if (node.ternaryExpr) node.ternaryExpr->accept(*this);
            indentLevel--;
            break;
        default:
            append("UNKNOWN, line = " + std::to_string(node.line) + ")\n");
            break;
    }
}

// ------------------------------------
// visit(TernaryExpr):
// Тернарный оператор
// ------------------------------------
void ASTPrinterVisitor::visit(TernaryExpr &node) {
    append(indent() + "TernaryExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    append(indent() + "True expression:\n");
    indentLevel++;
    node.trueExpr->accept(*this);
    indentLevel--;

    append(indent() + "Condition:\n");
    indentLevel++;
    node.condition->accept(*this);
    indentLevel--;

    append(indent() + "False expression:\n");
    indentLevel++;
    node.falseExpr->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(IdExpr):
// Идентификатор
// ------------------------------------
void ASTPrinterVisitor::visit(IdExpr &node) {
    append(indent() + "IdExpr (line = " + std::to_string(node.line) + "): " + node.name + "\n");
}

// ------------------------------------
// visit(LiteralExpr):
// Литерал: int, float, string, bool, None
// ------------------------------------
void ASTPrinterVisitor::visit(LiteralExpr &node) {
    std::string typeStr;
    std::string valStr;

    // Определяем, какой тип лежит в variant
    if (std::holds_alternative<int>(node.value)) {
        typeStr = "INT";
        valStr = std::to_string(std::get<int>(node.value));
    }
    else if (std::holds_alternative<double>(node.value)) {
        typeStr = "FLOAT";
        valStr = std::to_string(std::get<double>(node.value));
    }
    else if (std::holds_alternative<std::string>(node.value)) {
        typeStr = "STRING";
        valStr = std::get<std::string>(node.value);
    }
    else if (std::holds_alternative<bool>(node.value)) {
        typeStr = "BOOL";
        valStr = (std::get<bool>(node.value) ? "True" : "False");
    }
    else if (std::holds_alternative<std::monostate>(node.value)) {
        typeStr = "NONE";
        valStr = "None";
    }

    append(indent() + "LiteralExpr (type = " + typeStr + ", line = " + std::to_string(node.line) + "): " + valStr + "\n");
}

// ------------------------------------
// visit(CallExpr):
// Вызов функции
// ------------------------------------
void ASTPrinterVisitor::visit(CallExpr &node) {
    append(indent() + "CallExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // Печатаем, что внутри – вызывающий объект
    append(indent() + "Caller:\n");
    indentLevel++;
    node.caller->accept(*this);
    indentLevel--;

    // А здесь – аргументы
    append(indent() + "Arguments:\n");
    indentLevel++;
    for (auto &arg : node.arguments) {
        arg->accept(*this);
    }
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(IndexExpr):
// Операция индексирования: base[index]
// ------------------------------------
void ASTPrinterVisitor::visit(IndexExpr &node) {
    append(indent() + "IndexExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    append(indent() + "Base:\n");
    indentLevel++;
    node.base->accept(*this);
    indentLevel--;

    append(indent() + "Index:\n");
    indentLevel++;
    node.index->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(AttributeExpr):
// Доступ к атрибуту: obj.name
// ------------------------------------
void ASTPrinterVisitor::visit(AttributeExpr &node) {
    append(indent() + "AttributeExpr (line = " + std::to_string(node.line) + "): ." + node.name + "\n");
    indentLevel++;

    append(indent() + "Object:\n");
    indentLevel++;
    node.obj->accept(*this);
    indentLevel--;

    indentLevel--;
}

// ------------------------------------
// visit(ListExpr):
// Литерал списка: [e1, e2, ...]
// ------------------------------------
void ASTPrinterVisitor::visit(ListExpr &node) {
    append(indent() + "ListExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    for (auto &el : node.elems) {
        el->accept(*this);
    }
    indentLevel--;
}

// ------------------------------------
// visit(SetExpr):
// Литерал множества: {e1, e2, ...}
// ------------------------------------
void ASTPrinterVisitor::visit(SetExpr &node) {
    append(indent() + "SetExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    for (auto &el : node.elems) {
        el->accept(*this);
    }
    indentLevel--;
}

// ------------------------------------
// visit(DictExpr):
// Литерал словаря: {k1: v1, k2: v2, ...}
// ------------------------------------
void ASTPrinterVisitor::visit(DictExpr &node) {
    append(indent() + "DictExpr (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;
    for (auto &kv : node.items) {
        append(indent() + "Key:\n");
        indentLevel++;
        kv.first->accept(*this);
        indentLevel--;

        append(indent() + "Value:\n");
        indentLevel++;
        kv.second->accept(*this);
        indentLevel--;
    }
    indentLevel--;
}

// ------------------------------------
// *** НОВЫЙ МЕТОД ***
// visit(ClassDecl):
// Объявление класса: имя, базовые классы, поля и методы
// ------------------------------------
void ASTPrinterVisitor::visit(ClassDecl &node) {
    // Печатаем саму сигнатуру class: имя, базовые (если есть), строка объявления
    std::string basesList;
    if (!node.baseClasses.empty()) {
        basesList = "(";
        for (size_t i = 0; i < node.baseClasses.size(); ++i) {
            basesList += node.baseClasses[i];
            if (i + 1 < node.baseClasses.size()) {
                basesList += ", ";
            }
        }
        basesList += ")";
    }

    append(indent() + "ClassDecl (line = " + std::to_string(node.line) + "): " + node.name + basesList + "\n");
    indentLevel++;

    // Печатаем поля (FieldDecl)
    if (!node.fields.empty()) {
        append(indent() + "Fields:\n");
        indentLevel++;
        for (auto &fld : node.fields) {
            // У FieldDecl нет собственного visit, поэтому печатаем вручную
            append(indent() + "FieldDecl (line = " + std::to_string(fld->line) + "): " + fld->name + "\n");
            if (fld->initExpr) {
                indentLevel++;
                append(indent() + "Initializer:\n");
                indentLevel++;
                fld->initExpr->accept(*this);
                indentLevel -= 2;
            }
        }
        indentLevel--;
    }

    // Печатаем методы
    if (!node.methods.empty()) {
        append(indent() + "Methods:\n");
        indentLevel++;
        for (auto &mth : node.methods) {
            mth->accept(*this); // это уже FuncDecl::visit
        }
        indentLevel--;
    }

    indentLevel--;
}


void ASTPrinterVisitor::visit(ListComp &node) {
    // Печатаем строку, указывая тип узла и номер строки
    append(indent() + "ListComp (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // 1) Печатаем, что за выражение создаётся на каждой итерации
    append(indent() + "Value expression:\n");
    indentLevel++;
    if (node.valueExpr) {
        node.valueExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    // 2) Печатаем имя переменной-итератора
    append(indent() + "Iterator variable: \"" + node.iterVar + "\"\n");

    // 3) Печатаем, по чему итерируемся
    append(indent() + "Iterable expression:\n");
    indentLevel++;
    if (node.iterableExpr) {
        node.iterableExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    indentLevel--;
}


// ------------------------------------
// visit(DictComp):
// Компрехеншен словаря: { keyExpr : valueExpr for iterVar in iterableExpr }
// ------------------------------------
void ASTPrinterVisitor::visit(DictComp &node) {
    // Печатаем строку, указывая тип узла и номер строки
    append(indent() + "DictComp (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // 1) Ключевое выражение
    append(indent() + "Key expression:\n");
    indentLevel++;
    if (node.keyExpr) {
        node.keyExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    // 2) Выражение-значение
    append(indent() + "Value expression:\n");
    indentLevel++;
    if (node.valueExpr) {
        node.valueExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    // 3) Имя переменной-итератора
    append(indent() + "Iterator variable: \"" + node.iterVar + "\"\n");

    // 4) Выражение iterable
    append(indent() + "Iterable expression:\n");
    indentLevel++;
    if (node.iterableExpr) {
        node.iterableExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    indentLevel--;
}


// ------------------------------------
// visit(TupleComp):
// Компрехеншен кортежа: ( valueExpr for iterVar in iterableExpr )
// (внутри реализован как список)
// ------------------------------------
void ASTPrinterVisitor::visit(TupleComp &node) {
    // Печатаем строку, указывая тип узла и номер строки
    append(indent() + "TupleComp (line = " + std::to_string(node.line) + "):\n");
    indentLevel++;

    // 1) Печатаем выражение, которое будет вычисляться и добавляться в результирующую коллекцию
    append(indent() + "Value expression:\n");
    indentLevel++;
    if (node.valueExpr) {
        node.valueExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    // 2) Имя переменной-итератора
    append(indent() + "Iterator variable: \"" + node.iterVar + "\"\n");

    // 3) Выражение iterable
    append(indent() + "Iterable expression:\n");
    indentLevel++;
    if (node.iterableExpr) {
        node.iterableExpr->accept(*this);
    } else {
        append(indent() + "(null)\n");
    }
    indentLevel--;

    indentLevel--;
}


// В файле printer.cpp, внутри класса ASTPrinterVisitor, добавляем следующий метод:

void ASTPrinterVisitor::visit(LambdaExpr &node) {
    // Печатаем саму лямбда-функцию: указываем, что это LambdaExpr и на какой строке она начинается.
    append(indent() + "LambdaExpr (line = " + std::to_string(node.line) + "):\n");

    // Переходим на уровень выше вложенности для параметров и тела
    indentLevel++;

    // Если у лямбды есть параметры, выводим их в понятном виде
    if (!node.params.empty()) {
        // Печатаем заголовок для списка параметров
        append(indent() + "Parameters:\n");
        // Увеличиваем отступ, чтобы параметры визуально были вложены
        indentLevel++;
        // Перебираем все имена параметров
        for (size_t i = 0; i < node.params.size(); ++i) {
            const std::string &paramName = node.params[i];
            // Печатаем каждое имя параметра на своей строке
            append(indent() + paramName + "\n");
        }
        // Уменьшаем отступ после списка параметров
        indentLevel--;
    }
    else {
        // Если у лямбды нет параметров (пустая lambda), можно явно это отметить
        append(indent() + "(no parameters)\n");
    }

    // Теперь печатаем тело лямбды
    append(indent() + "Body:\n");
    // Увеличиваем отступ перед выводом тела
    indentLevel++;
    if (node.body) {
        // Делегируем печать того, что внутри body (Expression)
        node.body->accept(*this);
    }
    else {
        // На всякий случай, если тело вдруг отсутствует (например, nullptr),
        // выводим просто пустое место или None
        append(indent() + "(empty body)\n");
    }
    // Снижаем отступ после тела
    indentLevel--;

    // Возвращаемся к уровню вложенности, существовавшему перед visit(LambdaExpr)
    indentLevel--;
}

void ASTPrinterVisitor::visit(LenStat &node) {
    // Печатаем сам узел LenStat с указанием строки
    // indent() вернёт нужное количество пробелов согласно текущему уровню вложенности
    append(indent() + "LenStat (line = " + std::to_string(node.line) + "):\n");

    // Заузливаемся на один уровень вложенности, чтобы распечатать вложенное выражение
    indentLevel++;

    // Проверяем, что expr действительно указано (по идее, должно быть всегда)
    if (node.expr) {
        // Распечатываем само выражение, делегируя дальнейший вывод подходящему visit(...)
        node.expr->accept(*this);
    }

    // Возвращаемся на предыдущий уровень вложенности
    indentLevel--;
}

// ------------------------------------
// visit(DirStat):
// Оператор dir <expr>
// ------------------------------------
void ASTPrinterVisitor::visit(DirStat &node) {
    // Печатаем сам узел DirStat с указанием строки
    append(indent() + "DirStat (line = " + std::to_string(node.line) + "):\n");

    // Погружаемся внутрь, чтобы распечатать вложенное выражение
    indentLevel++;

    if (node.expr) {
        node.expr->accept(*this);
    }

    // Выход на уровень выше
    indentLevel--;
}

// ------------------------------------
// visit(EnumerateStat):
// Оператор enumerate <expr>
// ------------------------------------
void ASTPrinterVisitor::visit(EnumerateStat &node) {
    // Печатаем сам узел EnumerateStat с указанием строки
    append(indent() + "EnumerateStat (line = " + std::to_string(node.line) + "):\n");

    // Переходим внутрь для печати выражения
    indentLevel++;

    if (node.expr) {
        node.expr->accept(*this);
    }

    // Возвращаемся к предыдущему уровню
    indentLevel--;
}