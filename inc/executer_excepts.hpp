#pragma once

// Простой «носитель» для возврата из функции в рантайме:
struct ReturnException {
    // сюда запихиваем значение, которое return должен вернуть
    std::shared_ptr<Object> value; // или ObjectPtr, если так у вас определено
};

// Исключения для break и continue:
struct BreakException  { };
struct ContinueException { };
