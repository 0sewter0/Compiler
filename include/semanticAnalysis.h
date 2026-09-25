#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"

class SemanticError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SemanticAnalyzer {
public:
    void analyze(const std::vector<std::unique_ptr<ASTNode>>& statements);

private:
    struct VariableInfo {
        std::string typeName;
        bool isArray = false;
    };

    struct FunctionInfo {
        std::string returnType;
        std::vector<std::string> paramTypes;
    };

    std::vector<std::unordered_map<std::string, VariableInfo>> scopes_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> structs_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::vector<std::string> functionStack_;

    bool inLoop_ = false;

    void enterScope();

    void leaveScope();

    void declareVariable(const std::string& name, const std::string& typeName, bool isArray = false);
    const VariableInfo* lookupVariable(const std::string& name) const;

    void declareFunction(const std::string& name, const std::string& returnType, std::vector<std::string> paramTypes);
    const FunctionInfo* lookupFunction(const std::string& name) const;

    std::string typeOf(const ExprNode* expr) const;
    std::string typeOf(const ASTNode* node) const;

    bool isNumericType(const std::string& typeName) const;
    bool isAssignable(const ExprNode* expr) const;

    void ensureCompatibleTypes(const std::string& leftType, const std::string& rightType, const std::string& opName) const;

    void validateNode(const ASTNode* node);

    void validateExpr(const ExprNode* expr);

    void validateStatement(const ASTNode* stmt);

    void validateBlock(const BlockAST& block);
    
    void validateFunction(const FunctionAST& function);
};
