#include <string>
#include <iostream>

#include "builtin.h"
#include "lexer.h"
#include "parser.h"
#include "SymbTable.h"
#include "ast.h"

llvm::LLVMContext Context;
llvm::IRBuilder<> Builder(Context);
std::unique_ptr<llvm::Module> TheModule;
SymbolTable symbolTable;
std::vector<LoopBlocks> LoopStack;

int main() {
    TheModule = std::make_unique<llvm::Module>("SRZCompiler", Context);
    initBuiltins(TheModule.get());

    std::string sourceCode = 
    "int main() {"
    "    int arr[18];"
    "    arr[17] = 412;"
    "    return arr[17];"
    "}";
    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    for(const auto &tok : tokens) {
        std::cout << "token: type=" << static_cast<int>(tok.type) << " | Lexeme='" << tok.lexeme << "'\n";
    }

    Parser parser(tokens);

    while(parser.peek().type != TokenType::Eof) {
        if(auto node = parser.parseDefinition()) {
            node->codegen();
        } else {
            std::cerr << "Parsing error encountered\n";
            break;
        }
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest("output.ll", ec);
    TheModule->print(dest, nullptr);

    std::cout << "success\n";
    return 0;
}