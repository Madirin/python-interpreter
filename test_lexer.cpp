#include <iostream>
#include <vector>
#include "lexer.hpp"

int main() {
    // Пример Python-кода для теста
    std::string code = R"(
def hello():
    print("Hello, world!")
    x = 42
    if x == 42:
        print("The answer!")
    print("no answer")
exit()
)";

    Lexer lexer(code);
    std::vector<Token> tokens = lexer.tokenize();

    // Вывод токенов
    for (const auto& token : tokens) {
        std::cout << "Token("
                  << static_cast<int>(token.type) << ", "  // int cast чтобы увидеть enum numeric value
                  << "\"" << token.value << "\", "
                  << "line=" << token.line << ", "
                  << "column=" << token.column << ")\n";
    }

    return 0;
}
