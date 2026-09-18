#include "codegenHelpers.h"

void endLifetime(const SymbolInfo& symbolInfo) {
    if(symbolInfo.Alloca && symbolInfo.lifetimeSize) {
        Builder.CreateLifetimeEnd(symbolInfo.Alloca, symbolInfo.lifetimeSize);
    }
}

void endLifetimes(const std::vector<SymbolInfo>& variables) {
    for(auto it = variables.rbegin(); it != variables.rend(); ++it) {
        endLifetime(*it);
    }
}

llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* TheFunction, const std::string& VarName, llvm::Type* VarTy) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(VarTy, nullptr, VarName);
}

llvm::Value* CreateArrayElementPointer(const SymbolInfo& arrayInfo,llvm::Value* index, const std::string& resultName) {
    if(!arrayInfo.Alloca || !index) {
        throw std::runtime_error("Cannot index an unknown array");
    }

    llvm::Type* allocatedType = arrayInfo.Alloca->getAllocatedType();

    switch(arrayInfo.variableType) {
        case VariableType::StaticArray:
            return Builder.CreateGEP(
                allocatedType,
                arrayInfo.Alloca,
                {Builder.getInt32(0), index},
                resultName);

        case VariableType::RuntimeArray:
            return Builder.CreateGEP(
                allocatedType,
                arrayInfo.Alloca,
                {index},
                resultName);

        case VariableType::Basic:
            throw std::runtime_error("Cannot index a non array variable");
    }

    throw std::runtime_error("Unknown array storage type");
}

llvm::Type* getTypeByName(const std::string& name) {
    if(name == "int") return Builder.getInt32Ty();
    if(name == "float") return Builder.getFloatTy();
    if(auto* sInfo = symbolTable.getStructInfo(name)) {
        return sInfo->type;
    }
    throw std::runtime_error("Unknown type: " + name);
}