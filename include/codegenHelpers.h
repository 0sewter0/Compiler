#pragma once
#include "include/ast.h"

llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* TheFunction, const std::string& VarName, llvm::Type* VarTy);
llvm::Type* getTypeByName(const std::string& name);
llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* TheFunction, const std::string& VarName, llvm::Type* VarTy);
llvm::Value* createArrayElementPointer(const SymbolInfo& arrayInfo,llvm::Value* index, const std::string& resultName);
void endLifetime(const SymbolInfo& symbolInfo);
void endLifetimes(const std::vector<SymbolInfo>& variables);