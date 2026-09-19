#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#include "builtin.h"
#include "lexer.h"
#include "include/parser.h"
#include "SymbTable.h"
#include "ast.h"

llvm::LLVMContext Context;
llvm::IRBuilder<> Builder(Context);
std::unique_ptr<llvm::Module> TheModule;
SymbolTable symbolTable;
std::vector<LoopBlocks> LoopStack;

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if(!file.is_open()) {
        std::cerr << "Error: Couldn't open file " << filename << std::endl;
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string getExecutableDir(const std::string &execPath) {
    size_t lastSlash = execPath.find_last_of("/\\");
    if(lastSlash == std::string::npos) {
        return "";
    }
    return execPath.substr(0, lastSlash + 1);
}

int main(int argc, char* argv[]) {
    std::string filename = getExecutableDir(argv[0]) + "test.zH";

    if(argc > 1) {
        filename = argv[1];
    }

    std::string sourceCode = readFile(filename);

    TheModule = std::make_unique<llvm::Module>("SRZCompiler", Context);
    initBuiltins(TheModule.get());

    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    for(const auto &tok : tokens) {
        std::cout << "token: type=" << static_cast<int>(tok.type) << " | Lexeme='" << tok.lexeme << "'\n";
    }

    Parser parser(tokens);

    while(parser.peek().type != TokenType::Eof) {
        if(auto node = parser.parseTopLevel()) {
            node->codegen();
        } else {
            std::cerr << "Parsing error encountered at token: " << parser.peek().lexeme << std::endl;
            break;
        }
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest("output.ll", ec);
    TheModule->print(dest, nullptr);

    std::cout << "success\n";
    return 0;
}