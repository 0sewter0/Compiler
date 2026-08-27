#pragma once
#include "ast.h"

void initBuiltins(llvm::Module* module) {
    llvm::FunctionType* writeType = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), {llvm::Type::getInt32Ty(Context)}, false);

    llvm::Function::Create(writeType, llvm::Function::ExternalLinkage, "write", module);
}
