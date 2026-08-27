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

int main() {
    TheModule = std::make_unique<llvm::Module>("SRZCompiler", Context);
    initBuiltins(TheModule.get());

    std::string sourceCode = " { int x = 5; }";
    Lexer lexer(sourceCode);
    auto tokens = lexer.tokenize();

    for(const auto& tok : tokens) {
        std::cout << tok.lexeme;
    }
    std::cout << std::endl;

    Parser parser(tokens);
    auto ast = parser.parse();

    if(!ast) {
        return 1;
    }

    //Creating a wrapper-function for main() with the signature int32()
    llvm::FunctionType* FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), false);
    llvm::Function* MainFunc = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "main", TheModule.get());

    llvm::BasicBlock* BB = llvm::BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(BB);

    llvm::Value* RetVal = ast->codegen(); // generating IR from ast

    if(RetVal) {
        Builder.CreateRet(RetVal);
    } else {
        Builder.CreateRet(llvm::ConstantInt::get(Context, llvm::APInt(32, 0)));
    }

    std::error_code ec;
    llvm::raw_fd_ostream dest("output.ll", ec);
    TheModule->print(dest, nullptr);

    return 0;
}