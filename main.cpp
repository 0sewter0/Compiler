#include <string>
#include <iostream>
#include "lexer.h"
#include "parser.h"


int main() {
    std::string sourceCode = "2 + 3 * 4";

    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    auto ast = parser.parse();

    if(ast) {
        std::cout << "  Generated AST   \n";
        ast->print();
    } else {
        std::cout << "Error\n";
    }
    return 0;
}