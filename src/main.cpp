#include <iostream>
#include <vector>
#include "lexer.hpp"
#include <fstream>
#include "printer.hpp"
#include <cstdio>
#include "parser.hpp"

int main() {
    
    std::string file_name = "build/bin/test.py";
    std::ifstream file(file_name);
    if (!file) {
        std::cerr << "Cannot open file: " << file_name << std::endl;
        return 1;
    }
    std::string line;
    std::string code;
    std::cout<<"READ FROM FILE \n";
    while(std::getline(file, line)) {
        std::cout<<"line \n";
        code += line;
        code += '\n';
    }

    // for (char c: code) {
    //     printf("%d - %c\n", c, c);
    // }

    Lexer lexer(code);
    std::vector<Token> tokens = lexer.tokenize();
    std::cout<<"TEST \n";
    // Вывод токенов
    for (const auto& token : tokens) {
        std::cout << "Token("
                  << static_cast<int>(token.type) << ", "  
                  << "\"" << token.value << "\", "
                  << "line=" << token.line << ", "
                  << "column=" << token.column << ")\n";
    }

    std::cout << "\n=== Parsing ===" << std::endl;
    
    
    Parser parser(tokens);
    
    
    auto ast = parser.parse();

    ASTPrinterVisitor printer;
    ast->accept(printer);
    
    std::cout << printer.getResult() << std::endl;
    
    std::cout << "AST successfully generated." << std::endl;

    return 0;
}
