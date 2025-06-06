#include "parser.hpp"
#include <stdexcept>

// Конструктор парсера: принимает вектор токенов, начинаем с позиции 0
Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), cur(0)
{}

// Вспомогательные алиасы для типов возвращаемых узлов
using funcDecl    = std::unique_ptr<FuncDecl>;
using blockStat   = std::unique_ptr<BlockStat>;
using statement   = std::unique_ptr<Statement>;
using condStat    = std::unique_ptr<CondStat>;
using whileStat   = std::unique_ptr<WhileStat>;
using forStat     = std::unique_ptr<ForStat>;
using returnStat  = std::unique_ptr<ReturnStat>;
using breakStat   = std::unique_ptr<BreakStat>;
using continueStat= std::unique_ptr<ContinueStat>;
using passStat    = std::unique_ptr<PassStat>;
using assertStat  = std::unique_ptr<AssertStat>;
using exitStat    = std::unique_ptr<ExitStat>;
using exprStat    = std::unique_ptr<ExprStat>;
using printStat   = std::unique_ptr<PrintStat>;
using expression  = std::unique_ptr<Expression>;

// Возвращает текущий токен (не сдвигаясь)
Token Parser::peek() const {
    if (cur < tokens.size()) {
        return tokens[cur];
    }
    throw std::runtime_error("peek - нет токенов");
}

// Смотрим следующий токен (текущий + 1)
Token Parser::peek_next() const {
    if (cur + 1 < tokens.size()) {
        return tokens[cur + 1];
    }
    throw std::runtime_error("peek_next - нет следующего токена");
}

// «Съесть» текущий токен и перейти к следующему
Token Parser::advance() {
    if (!is_end()) {
        return tokens[cur++];
    }
    throw std::runtime_error("advance - некуда двигаться");
}

// Проверяем, что не дошли до конца последовательности
bool Parser::is_end() const {
    return cur >= tokens.size();
}

// Если текущий токен имеет любой из типов types, то «съесть» его и вернуть true
template<typename... Types>
bool Parser::match(Types... types) {
    if (is_end()) return false;
    TokenType curType = peek().type;
    if (( (curType == types) || ... )) {
        advance();
        return true;
    }
    return false;
}

// Извлечь токен точно указанного типа, иначе бросить исключение
Token Parser::extract(TokenType type) {
    if (is_end()) {
        throw std::runtime_error("extract - нет токенов");
    }
    if (peek().type != type) {
        throw std::runtime_error(
            "extract - ожидался токен другого типа, получили: " + peek().value
        );
    }
    return advance();
}


// -------------------------
// Главный метод: парсим весь вход и возвращаем корень AST (TransUnit)
// -------------------------
std::unique_ptr<TransUnit> Parser::parse() {
    // Если нам нужно хранить строку, где начинается вся единица перевода,
    // можно взять номер первого токена.
    int tu_line = is_end() ? 0 : peek().line;
    auto translationUnit = std::make_unique<TransUnit>(tu_line);

    // Пока токены не кончились, пробуем разобрать или определение функции, или класса, или просто оператор
    while (!is_end()) {
        if (peek().type == TokenType::DEF) {
            translationUnit->units.push_back(parse_func_decl());
        }
        else if (peek().type == TokenType::CLASS) {
            translationUnit->units.push_back(parse_class_decl());
        }
        else {
            translationUnit->units.push_back(parse_stat());
        }
    }
    return translationUnit;
}


// -------------------------
// Разбор объявления функции
// <func_decl> = 'def' <id> '(' <param_decl>? (',' <param_decl>)* ')' ':' NEWLINE <block_st>
// -------------------------
funcDecl Parser::parse_func_decl() {
    // Съедаем «def»
    Token defTok = extract(TokenType::DEF);
    int def_line = defTok.line;

    // Далее идёт имя функции
    Token idtoken = extract(TokenType::ID);
    std::string funcname = idtoken.value;
    int id_line = idtoken.line; // обычно совпадает с def_line

    // Ожидаем '('
    extract(TokenType::LPAREN);

    // Списки для pos- и default-параметров
    std::vector<std::string> pos_params;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> def_params;
    parse_param_decl(pos_params, def_params);

    // Ожидаем ')'
    extract(TokenType::RPAREN);
    // Двоеточие
    extract(TokenType::COLON);
    // Обязательный переход на новую строку
    extract(TokenType::NEWLINE);

    // Тело функции должно быть с отступом
    blockStat body = parse_block();

    // Создаём узел FuncDecl, передаём всё, включая номер строки def_line
    return std::make_unique<FuncDecl>(
        funcname,
        std::move(pos_params),
        std::move(def_params),
        std::move(body),
        def_line
    );
}


// -------------------------
// Разбор объявления класса
// <class_decl> = 'class' ID ('(' ID (',' ID)* ')')? ':' NEWLINE <block_st>
// -------------------------
std::unique_ptr<ClassDecl> Parser::parse_class_decl() {
    Token classToken = extract(TokenType::CLASS);
    int class_line = classToken.line;

    // Имя класса
    Token nameTok = extract(TokenType::ID);
    std::string className = nameTok.value;

    // Опциональные базовые классы
    std::vector<std::string> bases;
    if (match(TokenType::LPAREN)) {
        do {
            Token baseTok = extract(TokenType::ID);
            bases.push_back(baseTok.value);
        } while (match(TokenType::COMMA));
        extract(TokenType::RPAREN);
    }

    // Двоеточие и переход на новую строку
    extract(TokenType::COLON);
    extract(TokenType::NEWLINE);

    // Ожидаем INDENT
    extract(TokenType::INDENT);

    // Внутри класса могут быть объявления полей и методов
    std::vector<std::unique_ptr<FieldDecl>> fields;
    std::vector<std::unique_ptr<FuncDecl>> methods;

    // Пока не встретим DEDENT
    while (!is_end() && peek().type != TokenType::DEDENT) {
        Token current = peek();

        // Если встретили «def», значит это метод
        if (current.type == TokenType::DEF) {
            auto methodPtr = parse_func_decl();
            methods.push_back(std::move(methodPtr));
        }
        // Если встретили ID, а дальше идёт '=', значит поле
        else if (current.type == TokenType::ID) {
            Token fieldNameTok = extract(TokenType::ID);
            std::string fieldName = fieldNameTok.value;
            int field_line = fieldNameTok.line;

            extract(TokenType::ASSIGN);

            auto initializerExpr = parse_expression();

            extract(TokenType::NEWLINE);

            // Объявляем FieldDecl, передаём line
            fields.push_back(std::make_unique<FieldDecl>(
                fieldName,
                std::move(initializerExpr),
                field_line
            ));
        }
        // Если встретили пустую строку
        else if (current.type == TokenType::NEWLINE) {
            advance();
        }
        // Любые другие токены — ошибка
        else {
            throw std::runtime_error(
                "Line " + std::to_string(current.line)
                + ": unexpected token in class body: " + current.value
            );
        }
    }

    // Съедаем DEDENT
    extract(TokenType::DEDENT);

    return std::make_unique<ClassDecl>(
        className,
        std::move(bases),
        std::move(fields),
        std::move(methods),
        class_line
    );
}


// -------------------------
// Разбор списка параметров: pos_params и default_params
// -------------------------
void Parser::parse_param_decl(
    std::vector<std::string> &pos_params,
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> &def_params
) {
    // Если сразу ')', то параметры отсутствуют
    if (peek().type == TokenType::RPAREN) {
        return;
    }

    do {
        // Съедаем имя параметра
        Token param_token = extract(TokenType::ID);
        std::string name = param_token.value;
        int param_line = param_token.line;

        // Если есть '=', значит default-параметр
        if (match(TokenType::ASSIGN)) {
            auto def_expr = parse_expression();
            def_params.emplace_back(name, std::move(def_expr));
        }
        // Иначе просто positional
        else {
            pos_params.push_back(name);
        }
    } while (match(TokenType::COMMA));
}


// -------------------------
// Разбор блока: ожидаем INDENT, потом набор операторов, потом DEDENT
// -------------------------
blockStat Parser::parse_block() {
    Token indentTok = extract(TokenType::INDENT);
    int block_line = indentTok.line;

    auto block = std::make_unique<BlockStat>(block_line);

    while (!is_end() && peek().type != TokenType::DEDENT) {
        block->statements.push_back(parse_stat());
    }

    // Если дошли до конца — возвращаем то, что есть
    if (is_end()) {
        return block;
    }

    // Съедаем DEDENT
    extract(TokenType::DEDENT);

    return block;
}


// -------------------------
// Разбор оператора (statement) на верхнем уровне
// -------------------------
statement Parser::parse_stat() {
    Token curTok = peek();

    // Если начинается с идентификатора — может быть несколько случаев
    if (curTok.type == TokenType::ID) {
        // 1) a[...] = ... 
        if (peek_next().type == TokenType::LBRACKET) {
            Token idtoken = extract(TokenType::ID);
            int id_line = idtoken.line;
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, id_line);

            extract(TokenType::LBRACKET);
            expression indexExpr = parse_expression();
            extract(TokenType::RBRACKET);

            if (!is_end() && peek().type == TokenType::ASSIGN) {
                advance(); // съели '='
                expression val = parse_expression();
                int assign_line = peek().line;
                extract(TokenType::NEWLINE);

                auto newIndexExpr = std::make_unique<IndexExpr>(
                    std::move(idexpr),
                    std::move(indexExpr),
                    assign_line
                );
                return std::make_unique<AssignStat>(
                    std::move(newIndexExpr),
                    std::move(val),
                    assign_line
                );
            }
            else {
                // Формируем выражение-инструкцию: idexpr[index] (возможно с цепочкой постфиксов)
                // потому что здесь нет '='
                // Как и раньше — пушим на парсинг первичного выражения.
                // Но поскольку мы уже собрали IndexExpr, делаем так:
                auto fullExpr = parse_postfix(std::move(idexpr));
                int expr_line = peek().line;
                extract(TokenType::NEWLINE);
                return std::make_unique<ExprStat>(
                    std::move(fullExpr),
                    expr_line
                );
            }
        }

        // 2) a.b = ... или a.b(...)...    
        //    Старый код сразу бросал скелет в parse_expr_stat(), что неправильно для вызова через точку.
        if (peek_next().type == TokenType::DOT) {
            // --- собираем первую часть a.b  ---
            Token idtoken = extract(TokenType::ID);
            int id_line = idtoken.line;
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, id_line);

            extract(TokenType::DOT);
            Token dot_id = extract(TokenType::ID);
            int dot_line = dot_id.line;

            auto newAttrExpr = std::make_unique<AttributeExpr>(
                std::move(idexpr),
                dot_id.value,
                dot_line
            );

            // --- Проверяем, идёт ли дальше операция присваивания (a.b = ...) ---
            if (!is_end() && peek().type == TokenType::ASSIGN) {
                advance(); // съели '='
                expression val = parse_expression();
                int assign_line = dot_line;
                extract(TokenType::NEWLINE);
                return std::make_unique<AssignStat>(
                    std::move(newAttrExpr),
                    std::move(val),
                    assign_line
                );
            }
            else {
                // --- Здесь НЕ присваивание, значит, либо вызов метода, либо просто чтение атрибута ---
                // Чтобы корректно распарсить цепочку post-fix (например Point.__init__(self, x, y)),
                // мы берём уже готовый AttributeExpr, выкатываем на него parse_postfix, а потом забираем NEWLINE.
                auto fullExpr = parse_postfix(std::move(newAttrExpr));
                int expr_line = peek().line;
                extract(TokenType::NEWLINE);
                return std::make_unique<ExprStat>(
                    std::move(fullExpr),
                    expr_line
                );
            }
        }

        // 3) Простое присваивание var = expr
        if (!is_end() && peek_next().type == TokenType::ASSIGN) {
            Token idtoken = extract(TokenType::ID);
            int id_line = idtoken.line;
            advance(); // съели '='
            expression val = parse_expression();
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, id_line);
            extract(TokenType::NEWLINE);
            return std::make_unique<AssignStat>(
                std::move(idexpr),
                std::move(val),
                id_line
            );
        }
        else {
            // 4) Всё, что не присваивание, разбираем как «выражение-инструкция»
            return parse_expr_stat();
        }
    }

    // --- остальные варианты: IF, WHILE, FOR, RETURN, BREAK, CONTINUE, PASS, ASSERT, EXIT, PRINT  ---
    if (curTok.type == TokenType::IF) {
        return parse_cond();
    }
    else if (curTok.type == TokenType::WHILE) {
        return parse_while();
    }
    else if (curTok.type == TokenType::FOR) {
        return parse_for();
    }
    else if (curTok.type == TokenType::RETURN) {
        return parse_return();
    }
    else if (curTok.type == TokenType::BREAK) {
        return parse_break();
    }
    else if (curTok.type == TokenType::CONTINUE) {
        return parse_continue();
    }
    else if (curTok.type == TokenType::PASS) {
        return parse_pass();
    }
    else if (curTok.type == TokenType::ASSERT) {
        return parse_assert();
    }
    else if (curTok.type == TokenType::EXIT) {
        return parse_exit();
    }
    else if (curTok.type == TokenType::PRINT) {
        return parse_print();
    }
    else {
        return parse_expr_stat();
    }
}


// -------------------------
// Разбор «выражение как оператор»
// <expr_st> = <expr> NEWLINE
// -------------------------
exprStat Parser::parse_expr_stat() {
    // Номер строки для ExprStat берём из начала выражения
    expression expr = parse_expression();
    int expr_line = peek().line;
    extract(TokenType::NEWLINE);
    return std::make_unique<ExprStat>(std::move(expr), expr_line);
}


// -------------------------
// Разбор условного оператора
// <conditional_st> = 'if' <expr> ':' <block_st> ('elif' <expr> ':' <block_st>)* ('else' ':' <block_st>)?
// -------------------------
condStat Parser::parse_cond() {
    Token ifTok = extract(TokenType::IF);
    int if_line = ifTok.line;

    expression cond = parse_expression();
    extract(TokenType::COLON);

    // После «:» ожидаем, что на новой строке будет INDENT
    if (peek().type == TokenType::NEWLINE) {
        extract(TokenType::NEWLINE);
    }
    blockStat ifblock = parse_block();
    auto cond_node = std::make_unique<CondStat>(
        std::move(cond),
        std::move(ifblock),
        if_line
    );

    // Разбираем все elif
    while (!is_end() && peek().type == TokenType::ELIF) {
        Token elifTok = advance();
        int elif_line = elifTok.line;

        expression elifcond = parse_expression();
        extract(TokenType::COLON);
        if (peek().type == TokenType::NEWLINE) {
            extract(TokenType::NEWLINE);
        }
        blockStat elifblock = parse_block();

        cond_node->elifblocks.push_back({
            std::move(elifcond),
            std::move(elifblock)
        });
    }

    // Разбираем else, если есть
    if (!is_end() && peek().type == TokenType::ELSE) {
        Token elseTok = advance();
        int else_line = elseTok.line;

        extract(TokenType::COLON);
        if (peek().type == TokenType::NEWLINE) {
            extract(TokenType::NEWLINE);
        }
        blockStat elseblock = parse_block();
        cond_node->elseblock = std::move(elseblock);
    }

    return cond_node;
}


// -------------------------
// Разбор while
// <while_st> = 'while' <expr> ':' NEWLINE <block_st>
// -------------------------
whileStat Parser::parse_while() {
    Token whileTok = extract(TokenType::WHILE);
    int while_line = whileTok.line;

    expression cond = parse_expression();
    extract(TokenType::COLON);

    // Переход на новую строку, если он есть
    if (!is_end() && peek().type == TokenType::NEWLINE) {
        extract(TokenType::NEWLINE);
    }

    blockStat whileblock = parse_block();
    return std::make_unique<WhileStat>(
        std::move(cond),
        std::move(whileblock),
        while_line
    );
}


// -------------------------
// Разбор for
// <for_st> = 'for' <id> (',' <id>)* 'in' <expr> ':' NEWLINE <block_st>
// -------------------------
forStat Parser::parse_for() {
    Token forTok = extract(TokenType::FOR);
    int for_line = forTok.line;

    std::vector<std::string> vars;
    do {
        Token id = extract(TokenType::ID);
        vars.push_back(id.value);
    } while (match(TokenType::COMMA));

    extract(TokenType::IN);

    expression iter = parse_expression();
    extract(TokenType::COLON);

    if (peek().type == TokenType::NEWLINE) {
        extract(TokenType::NEWLINE);
    }

    blockStat forblock = parse_block();
    return std::make_unique<ForStat>(
        std::move(vars),
        std::move(iter),
        std::move(forblock),
        for_line
    );
}


// -------------------------
// Разбор return
// <return_st> = 'return' <expr>? NEWLINE
// -------------------------
returnStat Parser::parse_return() {
    Token returnTok = extract(TokenType::RETURN);
    int return_line = returnTok.line;

    expression retExpr;
    if (!is_end() && peek().type != TokenType::NEWLINE) {
        retExpr = parse_expression();
    }

    extract(TokenType::NEWLINE);
    return std::make_unique<ReturnStat>(
        std::move(retExpr),
        return_line
    );
}


// -------------------------
// Разбор break
// <break_st> = 'break' NEWLINE
// -------------------------
breakStat Parser::parse_break() {
    Token breakTok = extract(TokenType::BREAK);
    int break_line = breakTok.line;
    extract(TokenType::NEWLINE);
    return std::make_unique<BreakStat>(break_line);
}


// -------------------------
// Разбор continue
// <continue_st> = 'continue' NEWLINE
// -------------------------
continueStat Parser::parse_continue() {
    Token continueTok = extract(TokenType::CONTINUE);
    int continue_line = continueTok.line;
    extract(TokenType::NEWLINE);
    return std::make_unique<ContinueStat>(continue_line);
}


// -------------------------
// Разбор pass
// <pass_st> = 'pass' NEWLINE
// -------------------------
passStat Parser::parse_pass() {
    Token passTok = extract(TokenType::PASS);
    int pass_line = passTok.line;
    extract(TokenType::NEWLINE);
    return std::make_unique<PassStat>(pass_line);
}


// -------------------------
// Разбор assert
// <assert_st> = 'assert' <expr> (',' <expr>)? NEWLINE
// -------------------------
assertStat Parser::parse_assert() {
    Token assertTok = extract(TokenType::ASSERT);
    int assert_line = assertTok.line;

    expression condExpr = parse_expression();
    std::unique_ptr<Expression> message = nullptr;

    if (match(TokenType::COMMA)) {
        message = parse_expression();
    }

    extract(TokenType::NEWLINE);

    return std::make_unique<AssertStat>(
        std::move(condExpr),
        std::move(message),
        assert_line
    );
}


// -------------------------
// Разбор exit
// <exit_st> = 'exit' '(' <expr>? ')' NEWLINE
// -------------------------
exitStat Parser::parse_exit() {
    Token exitTok = extract(TokenType::EXIT);
    int exit_line = exitTok.line;

    extract(TokenType::LPAREN);
    expression exit_expr = nullptr;
    if (!is_end() && peek().type != TokenType::RPAREN) {
        exit_expr = parse_expression();
    }
    extract(TokenType::RPAREN);
    extract(TokenType::NEWLINE);

    return std::make_unique<ExitStat>(
        std::move(exit_expr),
        exit_line
    );
}


// -------------------------
// Разбор print
// <print_st> = 'print' '(' <expr>? ')' NEWLINE
// -------------------------
printStat Parser::parse_print() {
    Token printTok = extract(TokenType::PRINT);
    int print_line = printTok.line;

    extract(TokenType::LPAREN);
    expression expr = nullptr;
    if (!is_end() && peek().type != TokenType::RPAREN) {
        expr = parse_expression();
    }
    extract(TokenType::RPAREN);
    extract(TokenType::NEWLINE);

    return std::make_unique<PrintStat>(
        std::move(expr),
        print_line
    );
}


// -------------------------
// Разбор выражения
// -------------------------
expression Parser::parse_expression() {
    return parse_or();
}


// <or_expr> = <and_expr> ('or' <and_expr>)* 
expression Parser::parse_or() {
    expression left = parse_and();

    while (!is_end() && peek().type == TokenType::OR) {
        Token opTok = advance();
        int or_line = opTok.line;
        std::string op = opTok.value;
        expression right = parse_and();
        left = std::make_unique<BinaryExpr>(
            std::move(left),
            op,
            std::move(right),
            or_line
        );
    }
    return left;
}


// <and_expr> = <not_expr> ('and' <not_expr>)* 
expression Parser::parse_and() {
    expression left = parse_not();

    while (!is_end() && peek().type == TokenType::AND) {
        Token opTok = advance();
        int and_line = opTok.line;
        std::string op = opTok.value;
        expression right = parse_not();
        left = std::make_unique<BinaryExpr>(
            std::move(left),
            op,
            std::move(right),
            and_line
        );
    }
    return left;
}


// <not_expr> = ('not')* <comparison_expr>
expression Parser::parse_not() {
    if (!is_end() && peek().type == TokenType::NOT) {
        Token opTok = advance();
        int not_line = opTok.line;
        std::string op = opTok.value;
        expression operand = parse_not();
        return std::make_unique<UnaryExpr>(
            op,
            std::move(operand),
            not_line
        );
    }
    return parse_comparison();
}


// <comparison_expr> = <arith_expr> (('==' | '!=' | '<' | '>' | '<=' | '>=') <arith_expr>)*
expression Parser::parse_comparison() {
    expression left = parse_arith();

    while (!is_end()) {
        Token opTok = peek();
        if (opTok.type == TokenType::EQUAL   ||
            opTok.type == TokenType::NOTEQUAL||
            opTok.type == TokenType::LESS    ||
            opTok.type == TokenType::GREATER ||
            opTok.type == TokenType::LESSEQUAL   ||
            opTok.type == TokenType::GREATEREQUAL)
        {
            Token taken = advance();
            int comp_line = taken.line;
            std::string op = taken.value;
            expression right = parse_arith();
            left = std::make_unique<BinaryExpr>(
                std::move(left),
                op,
                std::move(right),
                comp_line
            );
        } else {
            break;
        }
    }
    return left;
}


// <arith_expr> = <term> (('+' | '-' | '+=' | '-=') <term>)* 
expression Parser::parse_arith() {
    expression left = parse_term();

    while (!is_end() &&
          ( peek().type == TokenType::PLUS   ||
            peek().type == TokenType::MINUS  ||
            peek().type == TokenType::PLUSEQUAL ||
            peek().type == TokenType::MINUSEQUAL ))
    {
        Token opTok = advance();
        int arith_line = opTok.line;
        std::string op = opTok.value;
        expression right = parse_term();
        left = std::make_unique<BinaryExpr>(
            std::move(left),
            op,
            std::move(right),
            arith_line
        );
    }
    return left;
}


// <term> = <factor> (('*' | '/' | '//' | '%') <factor>)*
expression Parser::parse_term() {
    expression left = parse_factor();

    while (!is_end() &&
          ( peek().type == TokenType::STAR        ||
            peek().type == TokenType::SLASH       ||
            peek().type == TokenType::DOUBLESLASH ||
            peek().type == TokenType::MOD ))
    {
        Token opTok = advance();
        int term_line = opTok.line;
        std::string op = opTok.value;
        expression right = parse_factor();
        left = std::make_unique<BinaryExpr>(
            std::move(left),
            op,
            std::move(right),
            term_line
        );
    }
    return left;
}


// <factor> = (('+' | '-') <factor>)? <power>
expression Parser::parse_factor() {
    if (!is_end() &&
       ( peek().type == TokenType::PLUS ||
         peek().type == TokenType::MINUS ))
    {
        Token opTok = advance();
        int fact_line = opTok.line;
        std::string op = opTok.value;
        expression operand = parse_factor();
        return std::make_unique<UnaryExpr>(
            op,
            std::move(operand),
            fact_line
        );
    }
    else {
        return parse_power();
    }
}


// <power> = <primary> ('**' <factor>)?
expression Parser::parse_power() {
    expression base = parse_primary();

    if (!is_end() && peek().type == TokenType::POW) {
        Token opTok = advance();
        int pow_line = opTok.line;
        std::string op = opTok.value;
        expression right = parse_factor();
        base = std::make_unique<BinaryExpr>(
            std::move(base),
            op,
            std::move(right),
            pow_line
        );
    }
    return base;
}


// <primary> = <literal_expr> | <id_expr> | '(' <expr> ')' | <ternary_expr> | <list> | <dict_or_set>
expression Parser::parse_primary() {
    Token token = peek();

    // Целое число
    if (token.type == TokenType::INTNUM) {
        advance();
        int ival = std::stoi(token.value);
        return std::make_unique<LiteralExpr>(ival, token.line);
    }
    // Вещественное
    else if (token.type == TokenType::FLOATNUM) {
        advance();
        double dval = std::stod(token.value);
        return std::make_unique<LiteralExpr>(dval, token.line);
    }
    // Строковый литерал
    else if (token.type == TokenType::STRING) {
        advance();
        return std::make_unique<LiteralExpr>(token.value, token.line);
    }
    // Булевое
    else if (token.type == TokenType::BOOL) {
        advance();
        bool bval = (token.value == "True");
        return std::make_unique<LiteralExpr>(bval, token.line);
    }
    // None
    else if (token.type == TokenType::NONE) {
        advance();
        return std::make_unique<LiteralExpr>(token.line);
    }
    // Идентификатор (возможно с постфиксом: вызов, индекс, атрибут)
    else if (token.type == TokenType::ID) {
        Token idtoken = extract(TokenType::ID);
        auto idexpr = std::make_unique<IdExpr>(idtoken.value, idtoken.line);
        return parse_postfix(std::move(idexpr));
    }
    // Скобочка '(', значит либо (...), либо тернар
    else if (token.type == TokenType::LPAREN) {
        advance(); // съели '('
        expression inside = parse_expression();
        extract(TokenType::RPAREN);
        int paren_line = token.line;
        return std::make_unique<PrimaryExpr>(
            std::move(inside),
            ParenTag{},
            paren_line
        );
    }
    // Список [...]
    else if (token.type == TokenType::LBRACKET) {
        Token leftBracket = advance();
        int list_line = leftBracket.line;
        std::vector<std::unique_ptr<Expression>> elems;
        if (peek().type != TokenType::RBRACKET) {
            do {
                elems.push_back(parse_expression());
            } while (match(TokenType::COMMA));
        }
        extract(TokenType::RBRACKET);
        return std::make_unique<ListExpr>(std::move(elems), list_line);
    }
    // Словарь или множество { ... }
    else if (token.type == TokenType::LBRACE) {
        Token leftBrace = advance();
        int brace_line = leftBrace.line;
        if (peek().type == TokenType::RBRACE) {
            // Пустой dict или set? В Python '{}' — пустой dict. 
            // Чтобы отличать set() и {}, можно на уровне semantics,
            // но здесь положим в DictExpr пустой список пар.
            advance(); // съели '}'
            return std::make_unique<DictExpr>(
                std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>>{},
                brace_line
            );
        }

        // Смотрим дальше: есть ли ':' — тогда это словарь, иначе — множество
        // Сначала парсим первое выражение
        expression first = parse_expression();

        if (match(TokenType::COLON)) {
            // dict
            std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
            expression first_val = parse_expression();
            items.emplace_back(std::move(first), std::move(first_val));

            while (match(TokenType::COMMA)) {
                expression key = parse_expression();
                extract(TokenType::COLON);
                expression val = parse_expression();
                items.emplace_back(std::move(key), std::move(val));
            }
            extract(TokenType::RBRACE);
            return std::make_unique<DictExpr>(std::move(items), brace_line);
        }
        else {
            // set
            std::vector<std::unique_ptr<Expression>> elems;
            elems.push_back(std::move(first));
            while (match(TokenType::COMMA)) {
                elems.push_back(parse_expression());
            }
            extract(TokenType::RBRACE);
            return std::make_unique<SetExpr>(std::move(elems), brace_line);
        }
    }
    else {
        throw std::runtime_error(
            "Line " + std::to_string(token.line) +
            ": unexpected token в parse_primary(): " + token.value
        );
    }
}


// -------------------------
// Обработка постфиксов (индекс, атрибут, вызов) после IdExpr
// -------------------------
expression Parser::parse_postfix(expression given_id) {
    while (!is_end()) {
        if (peek().type == TokenType::LBRACKET) {
            Token leftTok = extract(TokenType::LBRACKET);
            int idx_line = leftTok.line;
            expression indexExpr = parse_expression();
            extract(TokenType::RBRACKET);
            given_id = std::make_unique<IndexExpr>(
                std::move(given_id),
                std::move(indexExpr),
                idx_line
            );
        }
        else if (peek().type == TokenType::DOT) {
            Token dotTok = extract(TokenType::DOT);
            int dot_line = dotTok.line;
            Token id = extract(TokenType::ID);
            given_id = std::make_unique<AttributeExpr>(
                std::move(given_id),
                id.value,
                dot_line
            );
        }
        else if (peek().type == TokenType::LPAREN) {
            Token leftParen = extract(TokenType::LPAREN);
            int call_line = leftParen.line;
            std::vector<std::unique_ptr<Expression>> arguments;
            if (peek().type != TokenType::RPAREN) {
                arguments.push_back(parse_expression());
                while (match(TokenType::COMMA)) {
                    arguments.push_back(parse_expression());
                }
            }
            extract(TokenType::RPAREN);
            given_id = std::make_unique<CallExpr>(
                std::move(given_id),
                std::move(arguments),
                call_line
            );
        }
        else {
            break;
        }
    }
    return given_id;
}