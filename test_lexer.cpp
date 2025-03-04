#include <iostream>
#include <vector>
#include "lexer.hpp"

int main() {
    
    std::string code = R"(
"""markdown slash"""
exit()
)";

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
