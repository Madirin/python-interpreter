#include <iostream>
#include <vector>
#include "lexer.hpp"
#include <fstream>
#include <cstdio>

int main() {
    
    std::string file_name = "test.py";
    std::ifstream file(file_name);
    std::string line;
    std::string code;
    while(std::getline(file, line)) {
        code += line;
    }

    // for (char c: code) {
    //     printf("%d - %c\n", c, c);
    // }

    Lexer lexer(code);
    std::vector<Token> tokens = lexer.tokenize();

    // Вывод токенов
    for (const auto& token : tokens) {
        std::cout << "Token("
                  << static_cast<int>(token.type) << ", "  
                  << "\"" << token.value << "\", "
                  << "line=" << token.line << ", "
                  << "column=" << token.column << ")\n";
    }

    return 0;
}
