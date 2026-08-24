#include <string>
#include <iostream>
#include "lexer.h"

int main() {
    std::string sourceCode = "int x = 42 + 10;";

    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    sourceCode.clear();
    sourceCode.shrink_to_fit();

    for(const auto &token : tokens) {
        std::cout << "Token [Line " << token.line << ", Col " << token.col << "]: Lexeme='" << token.lexeme << "'\n";
    }
    return 0;
}