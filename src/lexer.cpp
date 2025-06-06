#include "lexer.hpp"
#include <stdexcept>
#include <bits/stdc++.h>
//recom:
// match eof/end (index+size < input.size) DONE
// order:
// keywords
// literal
// operator
const std::unordered_set<std::string> two_ops = {
    "==", "!=", "<=", ">=", "//", "**", "+=", "-=", "*=", "/=",
    "%=", "&=", "|=", "^=", ">>", "<<"
};

const std::unordered_set<std::string> three_ops = {
    "**=", "//=", ">>=", "<<="
};

// not in, is not пробелы
// if - tokentype::if
const std::unordered_map<std::string, TokenType> Lexer::triggers = {
    {"in", TokenType::IN},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"not", TokenType::NOT},
    {"is", TokenType::IS},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"elif", TokenType::ELIF},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"def", TokenType::DEF},
    {"return", TokenType::RETURN},
    {"assert", TokenType::ASSERT},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"pass", TokenType::PASS},
    {"True", TokenType::BOOL}, 
    {"False", TokenType::BOOL},
    {"None", TokenType::NONE},
    {"exit", TokenType::EXIT},
    {"print", TokenType::PRINT},
    {"input", TokenType::INPUT},
    {"class", TokenType::CLASS}
};


const std::unordered_map<std::string, TokenType> operator_map = {
    // Арифметические операторы
    {"+", TokenType::PLUS},   {"-", TokenType::MINUS},   {"*", TokenType::STAR},   {"/", TokenType::SLASH},
    {"//", TokenType::DOUBLESLASH}, {"%", TokenType::MOD}, {"**", TokenType::POW}, {"=", TokenType::ASSIGN},

    // Сравнение
    {"==", TokenType::EQUAL}, {"!=", TokenType::NOTEQUAL}, {"<", TokenType::LESS}, {">", TokenType::GREATER},
    {"<=", TokenType::LESSEQUAL}, {">=", TokenType::GREATEREQUAL},

    // Составные операторы
    {"+=", TokenType::PLUSEQUAL}, {"-=", TokenType::MINUSEQUAL}, {"*=", TokenType::STAREQUAL},
    {"/=", TokenType::SLASHEQUAL}, {"//=", TokenType::DOUBLESLASHEQUAL}, {"%=", TokenType::MODEQUAL},
    {"**=", TokenType::POWEQUAL},

    // Битовые операторы
    {"&=", TokenType::AND}, {"|=", TokenType::OR}, {"^=", TokenType::NOT},
    {">>", TokenType::IS}, {"<<", TokenType::ISNOT}, {">>=", TokenType::IS}, {"<<=", TokenType::ISNOT},

    
    {"(", TokenType::LPAREN}, {")", TokenType::RPAREN},
    {"[", TokenType::LBRACKET}, {"]", TokenType::RBRACKET},
    {"{", TokenType::LBRACE}, {"}", TokenType::RBRACE},

    // Разделители
    {",", TokenType::COMMA}, {":", TokenType::COLON},
    {".", TokenType::DOT}
};

Lexer::Lexer(const std::string& input) : input(input) {
    indent_stack.push_back(0);  
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (index < input.size()) {
        tokens.push_back(extract());
    }

    return tokens;
}

bool Lexer::match(bool eof, std::size_t size = 0) {
    if (eof) {
        return index + size >= input.size();
    } else {
        return index + size < input.size();
    }
}

Token Lexer::extract() {
    // Если у нас после каждого LINE_START есть незакрытые DEDENT, выпустили бы их раньше
    // Поэтому, до любого реального разбора, очередь на сброс отступов уже пуста.

    while (index < input.size()) {
        // Во-первых, если мы в самом начале строки или встретили табуляцию,
        // нужно проверить, не появилась ли строка с изменённым отступом
        if (at_line_start || input[index] == '\t') {
            at_line_start = false; // сбросим флаг, дальше – не начало строки
            // Извлекаем токен, связанный с отступами (INDENT/DEDENT/NOCHANGE)
            Token indentTok = extract_indentation();
            // Если это действительно изменение уровня (INDENT или DEDENT), возвращаем сразу
            if (indentTok.type == TokenType::INDENT || indentTok.type == TokenType::DEDENT) {
                return indentTok;
            }
            // Если вернули NOCHANGE (помечено как NEWLINE), значит отступ остался прежним,
            // но мы специально не возвратили токен, чтобы перейти к обычному анализу
        }

        // После того, как проверили отступ, сразу проверяем, не кончилась ли очередь DEDENT
        if (!pending_indent_tokens.empty()) {
            Token ded = pending_indent_tokens.front();
            pending_indent_tokens.erase(pending_indent_tokens.begin());
            return ded;
        }

        char c = input[index];

        // 1) Разбор «новой строки» '\n'
        if (c == '\n') {
            return extract_newline();
        }

        // 2) Пропускаем пробелы (но не в начале строки: там пробелы были учтены в extract_indentation)
        if (c == ' ') {
            ++index;
            ++column;
            continue; // продолжаем цикл, возможно, снова призыв extract_indentation
        }

        // 3) Буква или '_' → идентификатор (или ключевое слово)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            return extract_identifier();
        }

        // 4) Цифра → число
        if (std::isdigit(static_cast<unsigned char>(c))) {
            return extract_number();
        }

        // 5) Кавычка '"' → строковый литерал
        if (c == '"') {
            return extract_string();
        }

        // 6) Любой пунктуационный символ → оператор/разделитель
        if (std::ispunct(static_cast<unsigned char>(c))) {
            return extract_operator();
        }

        // Если ни одно правило не подошло — ошибка
        throw std::runtime_error(std::string("Unexpected symbol '") + c + "' at line " +
                                 std::to_string(line) + ", column " + std::to_string(column));
    }

    // Мы вышли из while, значит index >= input.size()
    // Если очередь DEDENT пустая, возвращаем END (конец входа)
    return {TokenType::END, "", line, column};
}

// -----------------------------------------------------------------------------
// Обработка «новой строки»
// -----------------------------------------------------------------------------
Token Lexer::extract_newline() {
    // Съедаём '\n', обновляем координаты
    ++index;
    ++line;
    column = 1;
    // Помечаем, что следующий символ — начало новой строки
    at_line_start = true;

    return {TokenType::NEWLINE, "\\n", line - 1, column};
}

// -----------------------------------------------------------------------------
// Обработка отступов в начале строки:
//   - Считаем, сколько пробелов/табов до первого «непробельного» символа.
//   - Сравниваем с текущим верхом indent_stack.
//     * Если больше — выпускаем INDENT и пушим новый уровень в стэк.
//     * Если меньше — пока уровни в стэке больше текущих, вытаскиваем их и
//       накапливаем DEDENT в очередь.
//     * Если равно — возвращаем специальный токен NOCHANGE (в нашем случае мы
//       представляем его как NEWLINE, но в extract() мы его отбрасываем).
// -----------------------------------------------------------------------------
Token Lexer::extract_indentation() {
    // Считаем текущий уровень «пробельных» символов, переводя '\t' в 4 пробела
    int current_spaces = 0;
    int orig_index = index;
    int orig_column = column;

    while (match(false) && (input[index] == ' ' || input[index] == '\t')) {
        if (input[index] == '\t') {
            current_spaces += 4;
        } else {
            current_spaces += 1;
        }
        ++index;
        ++column;
    }

    // Если нашли не-пробельный символ или дошли до конца строки/файла:
    // current_spaces — наш реальный уровень отступа
    int previous_indent = indent_stack.back();

    if (current_spaces > previous_indent) {
        // Новый уровень – пушим в стек и выдаём INDENT
        indent_stack.push_back(current_spaces);
        return Token{TokenType::INDENT, "", line, orig_column};
    }

    if (current_spaces < previous_indent) {
        // Уровень уменьшился – нужно несколько DEDENT подряд, пока не совпадёт
        while (!indent_stack.empty() && indent_stack.back() > current_spaces) {
            indent_stack.pop_back();
            // Вместо немедленного возврата, мы накапливаем в очередь:
            pending_indent_tokens.push_back(
                Token{TokenType::DEDENT, "", line, orig_column}
            );
        }
        // После этого в extract() мы сразу вернём первый DEDENT из очереди
        return Token{TokenType::NEWLINE, "\\n", line, orig_column}; 
        // специальный маркер «отступы изменились, но токен не готов, разводим очередь»
    }

    // Если current_spaces == previous_indent, никаких новых отступов нет
    // Возвращаем «заглушку» NEWLINE, которую extract() просто отбрасывает и идёт дальше
    return Token{TokenType::NEWLINE, "\\n", line, orig_column};
}

// -----------------------------------------------------------------------------
// Извлечение идентификатора (ID или ключевое слово)
// -----------------------------------------------------------------------------
Token Lexer::extract_identifier() {
    std::size_t size = 0;
    int start_col = column;

    // Собираем все символы [A-Za-z0-9_]
    while (match(false, size) &&
           (std::isalnum(static_cast<unsigned char>(input[index + size])) || input[index + size] == '_')) {
        ++size;
        ++column;
    }

    std::string name(input, index, size);
    index += size;

    // Смотрим, не в таблице ли это ключевое слово
    auto it = triggers.find(name);
    if (it != triggers.end()) {
        return {it->second, name, line, start_col};
    }

    // Иначе это просто идентификатор
    return {TokenType::ID, name, line, start_col};
}

// -----------------------------------------------------------------------------
// Извлечение числа (целое или вещественное). Очень прямолинейно:
//   1) Собираем целую часть.
//   2) Если следующий символ – точка, собираем дробные.
//   3) Если следующий после дробных – e/E, собираем экспоненту.
//   4) Возвращаем INTNUM или FLOATNUM в зависимости от наличия точки/экспоненты.
// -----------------------------------------------------------------------------
Token Lexer::extract_number() {
    std::size_t size = 0;
    int start_col = column;

    // Читаем целую часть
    while (match(false, size) && std::isdigit(static_cast<unsigned char>(input[index + size]))) {
        ++size;
        ++column;
    }

    bool is_float = false;

    // Смотрим, есть ли точка
    if (match(false, size) && input[index + size] == '.') {
        is_float = true;
        ++size;
        ++column;
        // Собираем дробную часть
        if (match(false, size) && std::isdigit(static_cast<unsigned char>(input[index + size]))) {
            while (match(false, size) && std::isdigit(static_cast<unsigned char>(input[index + size]))) {
                ++size;
                ++column;
            }
        } else {
            // Было что-то вроде "123." — допишем "0"
            std::string val(input, index, size);
            index += size;
            return {TokenType::FLOATNUM, val + "0", line, start_col};
        }
    }

    // Смотрим на экспоненту
    if (match(false, size) && (input[index + size] == 'e' || input[index + size] == 'E')) {
        is_float = true;
        ++size;
        ++column;
        // Может быть + или -
        if (match(false, size) && (input[index + size] == '+' || input[index + size] == '-')) {
            ++size;
            ++column;
        }
        // Цифры за экспонентой
        if (match(false, size) && std::isdigit(static_cast<unsigned char>(input[index + size]))) {
            while (match(false, size) && std::isdigit(static_cast<unsigned char>(input[index + size]))) {
                ++size;
                ++column;
            }
        } else {
            throw std::runtime_error("Invalid float literal at line " + std::to_string(line));
        }
    }

    std::string value(input, index, size);
    index += size;
    if (is_float) {
        return {TokenType::FLOATNUM, value, line, start_col};
    } else {
        return {TokenType::INTNUM, value, line, start_col};
    }
}

// -----------------------------------------------------------------------------
// Извлечение строкового литерала в двойных кавычках, поддерживается экранирование.
// -----------------------------------------------------------------------------
Token Lexer::extract_string() {
    char quote = input[index]; // наверняка '"'
    int start_col = column;
    bool is_triple = false;

    // Проверяем, не тройные ли кавычки
    if (match(false, 2) && input[index + 1] == quote && input[index + 2] == quote) {
        is_triple = true;
        index += 3;
        column += 3;
    } else {
        // Обычная одиночная
        ++index;
        ++column;
    }

    std::string value;
    bool escape = false;

    while (true) {
        if (index >= input.size()) {
            throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
        }
        char c = input[index];
        if (!escape && c == quote) {
            // Если тройные, ожидаем три кавычки
            if (is_triple && match(false, 2) && input[index + 1] == quote && input[index + 2] == quote) {
                index += 3;
                column += 3;
                break;
            }
            if (!is_triple) {
                ++index;
                ++column;
                break;
            }
        }
        if (escape) {
            switch (c) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '"':  value += '"';  break;
                case '\'': value += '\''; break;
                case '\\': value += '\\'; break;
                case 'b':  value += '\b'; break;
                case 'f':  value += '\f'; break;
                default:   value += c;    break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else {
            value += c;
        }
        ++index;
        ++column;
        if (c == '\n') {
            ++line;
            column = 1;
        }
    }

    return {TokenType::STRING, value, line, start_col};
}

// -----------------------------------------------------------------------------
// Извлечение «оператора» или разделителя (включая двух- и трёхсимвольные комбинации).
// -----------------------------------------------------------------------------
Token Lexer::extract_operator() {
    int start_col = column;
    std::string op;
    op += input[index];

    std::size_t size = 1;

    // Проверяем «два символа»
    if (match(false, 1)) {
        std::string two = op + input[index + 1];
        if (two_ops.find(two) != two_ops.end()) {
            op = two;
            size = 2;
        }
    }

    // Проверяем «три символа»
    if (match(false, size + 1)) {
        std::string three = op + input[index + size];
        if (three_ops.find(three) != three_ops.end()) {
            op = three;
            size = 3;
        }
    }

    index += size;
    column += static_cast<int>(size);

    auto it = operator_map.find(op);
    if (it != operator_map.end()) {
        return {it->second, op, line, start_col};
    }
    throw std::runtime_error("Unknown operator \"" + op + "\" at line " + std::to_string(line));
}