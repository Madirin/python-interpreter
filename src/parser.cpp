#include "parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), cur(0) {}

using funcDecl = std::unique_ptr<FuncDecl>;
using blockStat = std::unique_ptr<BlockStat>;
using statement = std::unique_ptr<Statement>;
using condStat = std::unique_ptr<CondStat>;
using whileStat = std::unique_ptr<WhileStat>;
using forStat = std::unique_ptr<ForStat>;
using returnStat = std::unique_ptr<ReturnStat>;
using breakStat = std::unique_ptr<BreakStat>;
using continueStat = std::unique_ptr<ContinueStat>;
using passStat = std::unique_ptr<PassStat>;
using assertStat = std::unique_ptr<AssertStat>;
using exitStat = std::unique_ptr<ExitStat>;
using exprStat = std::unique_ptr<ExprStat>;
using printStat = std::unique_ptr<PrintStat>;
using expression = std::unique_ptr<Expression>;

Token Parser::peek() const {
    if (cur < tokens.size()) {
        return tokens[cur];
    }

    throw std::runtime_error("peek - don't have any tokens left");
}

Token Parser::peek_next() const {
    if (cur < tokens.size()) {
        return tokens[cur + 1];
    }

    throw std::runtime_error("peek - don't have any tokens left");
}

Token Parser::advance() {
    if (!is_end()) {
        return tokens[cur++];
    }
    throw std::runtime_error("advance - nowhere to advance");
}

bool Parser::is_end() const {
    return cur >= tokens.size(); 
}

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

Token Parser::extract(TokenType type) {
    if(is_end()) {
        throw std::runtime_error("extract - don't have tokens");
    }
    if (peek().type != type) {
        throw std::runtime_error("extract - wrong type");
    }

    return advance();
}

std::unique_ptr<TransUnit> Parser::parse() {
    auto translationUnit = std::make_unique<TransUnit>();

    // Пока токены не закончились, парсим верхнеуровневые конструкции
    while (!is_end()) {
        // Если текущий токен DEF, это определение функции
        if (peek().type == TokenType::DEF) {
            translationUnit->units.push_back(parse_func_decl());
        } else if (peek().type == TokenType::CLASS) {
            translationUnit->units.push_back(parse_class_decl());
        } else {
            // Иначе, парсим инструкцию (statement)
            translationUnit->units.push_back(parse_stat());
        }
    }

    return translationUnit;
}

funcDecl Parser::parse_func_decl() {
    extract(TokenType::DEF);

    Token idtoken = extract(TokenType::ID);
    
    std::string funcname = idtoken.value;

    int def_line = idtoken.line;

    extract(TokenType::LPAREN);

    std::vector<std::string> pos_params;
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> def_params;

    parse_param_decl(pos_params, def_params);

    extract(TokenType::RPAREN);

    extract(TokenType::COLON);

    extract(TokenType::NEWLINE);

    blockStat body = parse_block();

    return std::make_unique<FuncDecl>(funcname, pos_params, std::move(def_params), std::move(body), def_line);
}

std::unique_ptr<ClassDecl> Parser::parse_class_decl() {
    
    Token classToken = advance();
    int line = classToken.line;

    
    Token nameTok = extract(TokenType::ID);
    std::string name = nameTok.value;

    // (' A, B, ... ')
    std::vector<std::string> bases;
    if (match(TokenType::LPAREN)) {
        do {
            Token baseTok = extract(TokenType::ID);
            bases.push_back(baseTok.value);
        } while (match(TokenType::COMMA));
        extract(TokenType::RPAREN);
    }

    
    extract(TokenType::COLON);
    extract(TokenType::NEWLINE);

    
    auto body = parse_block();

    
    return std::make_unique<ClassDecl>(name, std::move(bases), std::move(body), line);
}

void Parser::parse_param_decl(
    std::vector<std::string> &pos_params, 
    std::vector<std::pair<std::string, std::unique_ptr<Expression>>> &def_params) {
    

    if (peek().type == TokenType::RPAREN) {
        return;
    }

    do {
        
        Token param_token = extract(TokenType::ID);
        std::string name = param_token.value;
        if (match(TokenType::ASSIGN)) {
            auto def_expr = parse_expression();
            def_params.emplace_back(name, std::move(def_expr));
        } else {
            pos_params.push_back(name);
        }
        
    } while (match(TokenType::COMMA));

}

blockStat Parser::parse_block() {

    Token Indent = extract(TokenType::INDENT);

    auto block = std::make_unique<BlockStat>();

    while(!is_end() && peek().type != TokenType::DEDENT) {
        block->statements.push_back(parse_stat());
    }

    if (is_end()) {
        return block;
    }

    extract(TokenType::DEDENT);
    
    return block;
}

// expression Parser::parse_id_expr()

statement Parser::parse_stat() {

    Token cur = peek();

    if (cur.type == TokenType::ID) { // a.b = 1 attribute_ref
        if (peek_next().type == TokenType::LBRACKET) { // a[1] = 1
            Token idtoken = extract(TokenType::ID);
            int assign_line = idtoken.line;
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, idtoken.line);
            extract(TokenType::LBRACKET);
            expression indexExpr = parse_expression();
            extract(TokenType::RBRACKET);
            if (!(is_end()) && peek().type == TokenType::ASSIGN) {
                advance();
                expression val = parse_expression();
                int line = peek().line;
                auto new_idexpr = std::make_unique<IndexExpr>(std::move(idexpr), std::move(indexExpr), line);
                extract(TokenType::NEWLINE);
                return std::make_unique<AssignStat>(std::move(new_idexpr), std::move(val), assign_line);

            }
            else {
                return parse_expr_stat();
            }
        }
        if (peek_next().type == TokenType::DOT) {
            Token idtoken = extract(TokenType::ID);
            int assign_line, idexpr_line = idtoken.line;
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, idtoken.line);
            advance();
            Token dot_id = extract(TokenType::ID); 
            if (!(is_end()) && peek().type == TokenType::ASSIGN) {
                advance();
                expression val = parse_expression();
                int line = peek().line;
                auto new_idexpr = std::make_unique<AttributeExpr>(std::move(idexpr), dot_id.value, line);
                extract(TokenType::NEWLINE);
                return std::make_unique<AssignStat>(std::move(new_idexpr), std::move(val), assign_line);
            }
            else {
                return parse_expr_stat();
            }
        }

        if (!(is_end()) && peek_next().type == TokenType::ASSIGN) {
            Token idtoken = extract(TokenType::ID);
            int assign_line, idexpr_line = idtoken.line;
            advance();
            expression val = parse_expression();
            auto idexpr = std::make_unique<IdExpr>(idtoken.value, idtoken.line);
            extract(TokenType::NEWLINE);
            return std::make_unique<AssignStat>(std::move(idexpr), std::move(val), assign_line);
        }
        else {
            return parse_expr_stat();
        }
    }

    if (cur.type == TokenType::IF) {
        return parse_cond();
    }

    else if (cur.type == TokenType::WHILE) {
        return parse_while();
    }

    else if (cur.type == TokenType::FOR) {
        return parse_for();
    }

    else if (cur.type == TokenType::RETURN) {
        return parse_return();
    }

    else if (cur.type == TokenType::BREAK) {
        return parse_break();
    }

    else if (cur.type == TokenType::CONTINUE) {
        return parse_continue();
    }

    else if (cur.type == TokenType::PASS) {
        return parse_pass();
    }
    
    else if (cur.type == TokenType::ASSERT) {
        return parse_assert();
    }

    else if (cur.type == TokenType::EXIT) {
        return parse_exit();
    }

    else if (cur.type == TokenType::PRINT) {
        return parse_print();
    }

    else {
        return parse_expr_stat();
    }
}

exprStat Parser::parse_expr_stat() {
    expression expr = parse_expression();

    // is_end
    extract(TokenType::NEWLINE);
    return std::make_unique<ExprStat>(std::move(expr));
}

// <conditional_st> = 'if' <expr> ':' <block_st> ('elif' <expr> ':' <block_st>)* ('else' ':' <block_st>)?
condStat Parser::parse_cond() {

    extract(TokenType::IF);
    expression cond = parse_expression();
    extract(TokenType::COLON);
    blockStat ifblock = parse_block();

    auto cond_node = std::make_unique<CondStat>(std::move(cond), std::move(ifblock));

    while(!(is_end()) && peek().type == TokenType::ELIF) {
        
        advance(); // elif

        expression elifcond = parse_expression();
        extract(TokenType::COLON);
        blockStat elifblock = parse_block();

        cond_node->elifblocks.push_back({std::move(elifcond), std::move(elifblock)});
    }

    if (!(is_end()) && peek().type == TokenType::ELSE ) {

        advance();

        extract(TokenType::COLON);
        blockStat elseblock = parse_block();
        cond_node->elseblock = std::move(elseblock);
    }

    return cond_node;
}

// <while_st> = 'while' <expr> ':' <block_st>
whileStat Parser::parse_while() {

    extract(TokenType::WHILE);
    expression cond = parse_expression();
    extract(TokenType::COLON);
    extract(TokenType::NEWLINE);
    blockStat whileblock = parse_block();
    return std::make_unique<WhileStat>(std::move(cond), std::move(whileblock));
}

// <for_st> = 'for' <id> 'in' <expr> ':' <block_st>
forStat Parser::parse_for() {
    
    extract(TokenType::FOR);
    
    std::vector<std::string> vars;
    do {
        Token id = extract(TokenType::ID);
        vars.push_back(id.value);
    } while (match(TokenType::COMMA));

    extract(TokenType::IN);
    expression iter = parse_expression();
    extract(TokenType::COLON);
    extract(TokenType::NEWLINE);
    blockStat forblock = parse_block();
    return std::make_unique<ForStat>(vars, std::move(iter), std::move(forblock));
}

// <return_st> = 'return' <expr>? NEWLINE
returnStat Parser::parse_return() {
    extract(TokenType::RETURN);
    expression retExpr;

    if (peek().type != TokenType::NEWLINE) {
        retExpr = parse_expression();
    }
    extract(TokenType::NEWLINE);
    return std::make_unique<ReturnStat>(std::move(retExpr));
}

breakStat Parser::parse_break() {
    extract(TokenType::BREAK);
    extract(TokenType::NEWLINE);
    return std::make_unique<BreakStat>();
}


continueStat Parser::parse_continue() {
    extract(TokenType::CONTINUE);
    extract(TokenType::NEWLINE);
    return std::make_unique<ContinueStat>();
}


passStat Parser::parse_pass() {
    extract(TokenType::PASS);
    extract(TokenType::NEWLINE);
    return std::make_unique<PassStat>();
}

// <assert_st> = 'assert' <expr> (',' <expr>)? NEWLINE
assertStat Parser::parse_assert() {
    extract(TokenType::ASSERT);
    expression assert_expr = parse_expression();
    
    if(match(TokenType::COMMA)) {
        expression assert_expr_two = parse_expression();
    }

    extract(TokenType::NEWLINE);

    return std::make_unique<AssertStat>(std::move(assert_expr));
}


// <exit_st> = 'exit' '(' <expr>? ')' NEWLINE

exitStat Parser::parse_exit() {
    extract(TokenType::EXIT);
    extract(TokenType::LPAREN);
    expression exit_expr;
    if (peek().type != TokenType::RPAREN) {
        exit_expr = parse_expression();
    }
    extract(TokenType::RPAREN);
    extract(TokenType::NEWLINE);
    return std::make_unique<ExitStat>(std::move(exit_expr));
}

printStat Parser::parse_print() {
    extract(TokenType::PRINT);
    extract(TokenType::LPAREN);
    expression expr;
    if (peek().type != TokenType::RPAREN) {
        expr = parse_expression();
    }
    extract(TokenType::RPAREN);
    extract(TokenType::NEWLINE);
    return std::make_unique<PrintStat>(std::move(expr));
}

expression Parser::parse_expression() {
    return parse_or();
}

// <or_expr> = <and_expr> ('or' <and_expr>)* 
expression Parser::parse_or() {
    expression left = parse_and();

    while (!(is_end()) && peek().type == TokenType::OR ) {
        int orline = peek().line;
        std::string op = advance().value;
        expression right = parse_and();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), orline);
    }

    return left;    
}

// <and_expr> = <not_expr> ('and' <not_expr>)*
expression Parser::parse_and() {
    expression left = parse_not();

    while (!(is_end()) && peek().type == TokenType::AND ) {
        int andline = peek().line;
        std::string op = advance().value;
        expression right = parse_not();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), andline);
    }

    return left;
}

// <not_expr> = ('not')* <comparison_expr>

expression Parser::parse_not() {

    if (!is_end() && peek().type == TokenType::NOT) {
        int uline = peek().line;
        std::string op = advance().value;          // "not"
        expression operand = parse_not();
        return std::make_unique<UnaryExpr>(op, std::move(operand), uline);
    }
    return parse_comparison();
}


// <comparison_expr> = <arith_expr> (('==' | '!=' | '<' | '>' | '<=' | '>=') <arith_expr>)*
expression Parser::parse_comparison() {

    expression left = parse_arith();
    
    while(!(is_end())) {
        Token op = peek();


        if (op.type == TokenType::EQUAL || op.type == TokenType::NOTEQUAL || op.type == TokenType::LESS ||
            op.type == TokenType::GREATER || op.type == TokenType::LESSEQUAL || op.type == TokenType::GREATEREQUAL) {
                int compline = peek().line;
                std::string op = advance().value;
                expression right = parse_arith();
                left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), compline);
        } else {
            break;
        }
    }

    return left;
}

// <arith_expr> = <term> (('+' | '-') <term>)*
expression Parser::parse_arith() {

    expression left = parse_term();

    while (!(is_end()) && (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS 
                        || peek().type == TokenType::PLUSEQUAL || peek().type == TokenType::MINUSEQUAL)) {

        int arithline = peek().line;
        std::string op = advance().value;
        expression right = parse_term();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), arithline);

    }

    return left;
}

// parse_term: <term> = <factor> (('*' | '/' | '//' | '%') <factor>)*
expression Parser::parse_term() {
    
    expression left = parse_factor();

    while(!(is_end()) && (peek().type == TokenType::STAR || peek().type == TokenType::SLASH ||
                          peek().type == TokenType::DOUBLESLASH || peek().type == TokenType::MOD)) {
        int termline = peek().line;
        std::string op = advance().value;
        expression right = parse_factor();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), termline);
    }

    return left;
}

// <factor> = (('+' | '-') <factor>)? <power>
expression Parser::parse_factor() {
    
    if (!is_end() && (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS)) {
        int uline = peek().line;
        std::string op = advance().value;
        expression operand = parse_factor();
        return std::make_unique<UnaryExpr>(op, std::move(operand), uline);
    } else {
        return parse_power();
    }
}

// parse_power: <power> = <primary> ('**' <factor>)?
expression Parser::parse_power() {
    
    expression base = parse_primary();
    
    if (!is_end() && peek().type == TokenType::POW) {
        int powline = peek().line;
        std::string op = advance().value; 
        expression right = parse_factor();
        base = std::make_unique<BinaryExpr>(std::move(base), op, std::move(right), powline);
    }

    return base;
}

// parse_primary: <primary> = <literal_expr> | <id_expr> | '(' <expr> ')' | <ternary_expr>

expression Parser::parse_primary() {
    Token token = peek();
    expression node;
    
    if (token.type == TokenType::INTNUM) {
        advance();
        int ival = std::stoi(token.value);
        return std::make_unique<LiteralExpr>(ival, token.line);
    }
    
    else if (token.type == TokenType::FLOATNUM) {
        advance();
        double dval = std::stod(token.value);
        return std::make_unique<LiteralExpr>(dval, token.line);
    }
    
    else if (token.type == TokenType::STRING) {
        advance();
        // token.value уже без кавычек, или вы можете удалить их здесь
        return std::make_unique<LiteralExpr>(token.value, token.line);
    }
    
    else if (token.type == TokenType::BOOL) {
        advance();
        bool bval = (token.value == "True");
        return std::make_unique<LiteralExpr>(bval, token.line);
    }
    
    else if (token.type == TokenType::NONE) {
        advance();
        return std::make_unique<LiteralExpr>(token.line); // моностейт
    }
    
    else if (token.type == TokenType::ID) {
        Token idtoken = extract(TokenType::ID);
        auto idexpr = std::make_unique<IdExpr>(idtoken.value, idtoken.line);
        return parse_postfix(std::move(idexpr));
    }
    
    // paren expr in parse_atom
    else if (token.type == TokenType::LPAREN) {
        advance(); 
        node = parse_expression();
        extract(TokenType::RPAREN); 
    }
    
    else if (token.type == TokenType::LBRACKET) {
        advance();
        std::vector<std::unique_ptr<Expression>> elems;
        if (peek().type != TokenType::RBRACKET) {
            do {
                elems.push_back(parse_expression());
            } while (match(TokenType::COMMA));
        } else {
            extract(TokenType::RBRACKET);
            return std::make_unique<ListExpr>(std::vector<std::unique_ptr<Expression>>{});
        }
        extract(TokenType::RBRACKET);
        return std::make_unique<ListExpr>(std::move(elems));
    }

    else if (token.type == TokenType::LBRACE) {
        advance();

        if (peek().type == TokenType::RBRACE) {
            advance();
            int line = peek().line;
            return std::make_unique<DictExpr>(std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>>{}, line);
        }

        expression first = parse_expression();

        if (match(TokenType::COLON)) { // dict
            std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> items;
            expression first_val = parse_expression();
            items.emplace_back(std::move(first), std::move(first_val));

            while(match(TokenType::COMMA)) {
                expression key = parse_expression();
                extract(TokenType::COLON);
                expression val = parse_expression();
                items.emplace_back(std::move(key), std::move(val));
            }

            int line = peek().line;
            extract(TokenType::RBRACE);
            return std::make_unique<DictExpr>(std::move(items), line);
        } else {
            std::vector<std::unique_ptr<Expression>> elems;
            elems.push_back(std::move(first));
            while (match(TokenType::COMMA)) {
                elems.push_back(parse_expression());
            }

            int line = peek().line;
            extract(TokenType::RBRACE);
            return std::make_unique<SetExpr>(std::move(elems), line);
        }
    }

    else {
        throw std::runtime_error("parse_primary - unexpected token: " + token.value);
    }

    return node;
}

expression Parser::parse_postfix(expression given_id) {
    while (!is_end()) {
        if (peek().type == TokenType::LBRACKET) {
            extract(TokenType::LBRACKET);
            expression indexExpr = parse_expression();
            extract(TokenType::RBRACKET);
            int line = peek().line;
            given_id = std::make_unique<IndexExpr>(std::move(given_id), std::move(indexExpr), line);
        } else if (peek().type == TokenType::DOT) {
            advance();
            Token id = extract(TokenType::ID);
            int line = peek().line;
            given_id = std::make_unique<AttributeExpr>(std::move(given_id), id.value, line);
        } else if (peek().type == TokenType::LPAREN){
            extract(TokenType::LPAREN);
            int line = peek().line;
            std::vector<std::unique_ptr<Expression>> arguments;
    
            if (peek().type != TokenType::RPAREN) {
                arguments.push_back(parse_expression());

                while (match(TokenType::COMMA)) {
                    arguments.push_back(parse_expression());
                }
            }

            extract(TokenType::RPAREN);    
            given_id = std::make_unique<CallExpr>(std::move(given_id), std::move(arguments), line);
        } else {
            break;
        }
    }
    return given_id;
}

expression Parser::parse_ternary() {
    
    expression expr = parse_primary();
    
    if (!is_end() && peek().type == TokenType::IF) {
        advance(); 
        expression cond = parse_expression();
        extract(TokenType::ELSE);
        expression falseExpr = parse_expression();
        return std::make_unique<TernaryExpr>(std::move(expr), std::move(cond), std::move(falseExpr));
    }
    
    return expr;
}

expression Parser::parse_call(expression caller) {
    
    extract(TokenType::LPAREN);
    
    std::vector<std::unique_ptr<Expression>> arguments;
    
    if (peek().type != TokenType::RPAREN) {
        arguments.push_back(parse_expression());

        while (match(TokenType::COMMA)) {
            arguments.push_back(parse_expression());
        }
    }

    int line = peek().line;
    extract(TokenType::RPAREN);    
    return std::make_unique<CallExpr>(std::move(caller), std::move(arguments), line);
}