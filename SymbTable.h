#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

#include "llvm/IR/Instructions.h"

struct StructTypeInfo {
    llvm::StructType* type;
    std::unordered_map<std::string, std::pair<unsigned, llvm::Type*>> fields;
};

struct SymbolInfo {
    llvm::AllocaInst* Alloca;
    std::string typeName;
};

class SymbolTable {
private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    std::unordered_map<std::string, StructTypeInfo> structTypes;
public:
    void pushScope() {
        scopes.emplace_back();
    }

    std::unordered_map<std::string, SymbolInfo> popScope() {
        if(scopes.empty()) throw std::runtime_error("No scopes to pop");

        auto PoppedScope = std::move(scopes.back());
        scopes.pop_back();
        return PoppedScope;
    }

    void registerStruct(const std::string &name, StructTypeInfo info) {
        structTypes[name] = info;
    }

    const StructTypeInfo* getStructInfo(const std::string &name) const {
        auto it = structTypes.find(name);
        if(it != structTypes.end()) return &it->second;
        return nullptr;
    }

    void declareVariable(const std::string& name, llvm::AllocaInst* alloca, const std::string& typeName = "") {
        
        if(scopes.empty()) throw std::runtime_error("No active scope for declaration");

        if(scopes.back().count(name)) {
            throw std::runtime_error("Redefinition of variable: " + name);
        }

        scopes.back()[name] = {alloca, typeName};
    }

    SymbolInfo lookupVariable(const std::string& name) {
        for(auto it = scopes.rbegin(); it != scopes.rend(); it++) {
            auto found = it->find(name);

            if(found != it->end())
                return found->second;
        }

        return {nullptr, ""};
    }
};

