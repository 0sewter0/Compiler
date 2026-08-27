#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

#include "llvm/IR/Instructions.h"

class SymbolTable {
private:
    // Scope
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes;

public:
    void pushScope() {
        scopes.push_back(std::unordered_map<std::string, llvm::AllocaInst*>());
    }

    std::unordered_map<std::string, llvm::AllocaInst*> popScope() {
        if (scopes.empty()) throw std::runtime_error("No scopes to pop");

        auto PoppedScope = std::move(scopes.back());

        return PoppedScope;
    }

    // Declare Variable in current scope
    void declareVariable(const std::string& name, llvm::AllocaInst* alloca) {
        if (scopes.empty()) throw std::runtime_error("No active scope for declaration");
        
        if (scopes.back().count(name) > 0) {
            throw std::runtime_error("Redefinition of variable: " + name);
        }
        
        scopes.back()[name] = alloca;
    }

    // variable search
    llvm::AllocaInst* lookupVariable(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) {
                return (*it)[name]; 
            }
        }
        return nullptr; // Undeclared Identifier
    }
};
