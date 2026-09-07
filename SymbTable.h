#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

#include "llvm/IR/Instructions.h"

class SymbolTable {
private:
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes;
public:
    void pushScope() {
        scopes.emplace_back();
    }

    std::unordered_map<std::string, llvm::AllocaInst*> popScope() {
        if(scopes.empty()) throw std::runtime_error("No scopes to pop");

        auto PoppedScope = std::move(scopes.back());
        scopes.pop_back();
        return PoppedScope;
    }

    void declareVariable(const std::string& name, llvm::AllocaInst* alloca) {
        
        if(scopes.empty()) throw std::runtime_error("No active scope for declaration");

        if(scopes.back().count(name)) {
            throw std::runtime_error("Redefinition of variable: " + name);
        }

        scopes.back()[name] = alloca;
    }

    llvm::AllocaInst* lookupVariable(const std::string& name) {
        for(auto it = scopes.rbegin(); it != scopes.rend(); it++) {
            auto found = it->find(name);

            if(found != it->end())
                return found->second;
        }

        return nullptr;
    }
};

