#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <stdexcept>

#include "builtin.h"
#include "lexer.h"
#include "include/parser.h"
#include "include/semanticAnalysis.h"
#include "SymbTable.h"
#include "ast.h"

llvm::LLVMContext Context;
llvm::IRBuilder<> Builder(Context);
std::unique_ptr<llvm::Module> TheModule;
SymbolTable symbolTable;
std::vector<LoopBlocks> LoopStack;

std::string readFile(const std::filesystem::path& filename) {
    std::ifstream file(filename);
    if(!file) {
        throw std::runtime_error("Couldn't open file: " + filename.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    std::filesystem::path filename = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(argv[0]).parent_path() / "test.zh";

    std::string sourceCode;
    try {
        sourceCode = readFile(filename);
    } catch(const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }

    TheModule = std::make_unique<llvm::Module>("SRZCompiler", Context);
    initBuiltins(TheModule.get());

    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);

    auto parsedProgram = parser.parse();
    for(const Error& error : diagnostic::errors()) {
        diagnostic::printAllErrors();
    }

    if(!diagnostic::errors().empty() || !parsedProgram) {
        return 1;
    }

    auto* program = dynamic_cast<ProgramAST*>(parsedProgram.get());
    if(!program) {
        std::cerr << "Internal error: parser did not produce a program AST.\n";
        return 1;
    }

    try {
        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.analyze(program->statements);
    } catch(const std::exception& error) {
        std::cerr << "Semantic error: " << error.what() << std::endl;
        return 1;
    }

    for(auto& node : program->statements) {
        if(node) {
            node->codegen();
        }
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest("output.ll", ec);
    if(ec) {
        std::cerr << "Error: Couldn't write output.ll: " << ec.message() << std::endl;
        return 1;
    }
    TheModule->print(dest, nullptr);

    std::cout << "success\n";
    return 0;
}