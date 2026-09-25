#pragma once
#include "ast.h"

void initBuiltins(llvm::Module* module) {
    auto int32Ty = llvm::Type::getInt32Ty(Context);

    llvm::FunctionType* writeType = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), {llvm::Type::getInt32Ty(Context)}, false);
    llvm::Function* writeFunc = llvm::Function::Create(writeType, llvm::Function::ExternalLinkage, "write", module);
    llvm::Function::Create(writeType, llvm::Function::ExternalLinkage, "write", module);

    symbolTable.declareFunction("write", writeFunc, int32Ty, { int32Ty });

    llvm::FunctionType* readType = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), {}, false);
    llvm::Function* readFunc = llvm::Function::Create(readType, llvm::Function::ExternalLinkage, "read", module);
    llvm::Function::Create(readType, llvm::Function::ExternalLinkage, "read", module);

    symbolTable.declareFunction("read", readFunc, int32Ty, { int32Ty });
}
