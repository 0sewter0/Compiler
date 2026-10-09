#include "include/semanticAnalysis.h"

namespace {
std::string getTypeNameForLiteral(const ExprNode* expr) {
    if (dynamic_cast<const NumberExprAST*>(expr)) return "int";
    if (dynamic_cast<const FloatExprAST*>(expr)) return "float";
    return "unknown";
}

std::unique_ptr<ExprNode> cloneExpr(const ExprNode* expr) {
    if(auto* node = dynamic_cast<const NumberExprAST*>(expr)) {
        return std::make_unique<NumberExprAST>(node->value);
    }
    if(auto* node = dynamic_cast<const FloatExprAST*>(expr)) {
        return std::make_unique<FloatExprAST>(node->value());
    }
    if(auto* node = dynamic_cast<const VariableExprAST*>(expr)) {
        return std::make_unique<VariableExprAST>(node->name);
    }
    if(auto* node = dynamic_cast<const BinaryExprAST*>(expr)) {
        return std::make_unique<BinaryExprAST>(node->op, cloneExpr(node->left.get()), cloneExpr(node->right.get()));
    }
    if(auto* node = dynamic_cast<const UnaryMinusExprAST*>(expr)) {
        return std::make_unique<UnaryMinusExprAST>(node->op, cloneExpr(node->operand.get()));
    }
    throw SemanticError("Unsupported expression in equation.");
}

struct LinearForm {
    std::unique_ptr<ExprNode> coefficient;
    std::unique_ptr<ExprNode> remainder;
    bool containsTarget = false;
};

std::optional<int> getIntegerConstant(const ExprNode* expr) {
    auto* number = dynamic_cast<const NumberExprAST*>(expr);
    if(!number) {
        return std::nullopt;
    }
    return number->value;
}

bool isNumber(const ExprNode* expr, int expected) {
    auto* number = dynamic_cast<const NumberExprAST*>(expr);
    return number && number->value == expected;
}

std::unique_ptr<ExprNode> simplify(std::unique_ptr<ExprNode> expr) {
    auto* binary = dynamic_cast<BinaryExprAST*>(expr.get());
    if(!binary) {
        return expr;
    }

    binary->left = simplify(std::move(binary->left));
    binary->right = simplify(std::move(binary->right));

    if(binary->op == "+") {
        if(isNumber(binary->left.get(), 0)) {
            return std::move(binary->right);
        }

        if(isNumber(binary->right.get(), 0)) {
            return std::move(binary->left);
        }
    }

    if(binary->op == "*") {
        if(isNumber(binary->left.get(), 0) ||
           isNumber(binary->right.get(), 0)) {
            return std::make_unique<NumberExprAST>(0);
        }

        if(isNumber(binary->left.get(), 1)) {
            return std::move(binary->right);
        }

        if(isNumber(binary->right.get(), 1)) {
            return std::move(binary->left);
        }
    }

    return expr;
}

std::unique_ptr<ExprNode> makeBinary(const std::string& op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) {
    return std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
}

LinearForm linearize(const ExprNode* expr, const std::string& target) {
    if(auto* variable = dynamic_cast<const VariableExprAST*>(expr)) {
        if(variable->name == target) {
            return {std::make_unique<NumberExprAST>(1), std::make_unique<NumberExprAST>(0), true};
        }
        return {std::make_unique<NumberExprAST>(0), cloneExpr(expr), false};
    }

    if(dynamic_cast<const NumberExprAST*>(expr) || dynamic_cast<const FloatExprAST*>(expr)) {
        return {std::make_unique<NumberExprAST>(0), cloneExpr(expr), false};
    }

    auto* unary = dynamic_cast<const UnaryMinusExprAST*>(expr);
    if(unary) {
        LinearForm operand = linearize(unary->operand.get(), target);
        return {
            makeBinary("-", std::make_unique<NumberExprAST>(0), std::move(operand.coefficient)),
            makeBinary("-", std::make_unique<NumberExprAST>(0), std::move(operand.remainder)),
            operand.containsTarget
        };
    }

    auto* binary = dynamic_cast<const BinaryExprAST*>(expr);
    if(!binary) {
        throw SemanticError("Only linear arithmetic is supported in equations.");
    }

    LinearForm left = linearize(binary->left.get(), target);
    LinearForm right = linearize(binary->right.get(), target);

    if(binary->op == "+" || binary->op == "-") {
        return {
            makeBinary(binary->op, std::move(left.coefficient), std::move(right.coefficient)),
            makeBinary(binary->op, std::move(left.remainder), std::move(right.remainder)),
            left.containsTarget || right.containsTarget
        };
    }

    if(binary->op == "*" || binary->op == "/") {
        if(left.containsTarget && right.containsTarget) {
            throw SemanticError("Non-linear equation: the unknown appears twice in a product or quotient.");
        }
        if(!left.containsTarget && !right.containsTarget) {
            return {std::make_unique<NumberExprAST>(0), cloneExpr(expr), false};
        }
        if(binary->op == "*" && left.containsTarget) {
            return {
                makeBinary("*", std::move(left.coefficient), cloneExpr(right.remainder.get())),
                makeBinary("*", std::move(left.remainder), std::move(right.remainder)),
                true
            };
        }
        if(binary->op == "*" && right.containsTarget) {
            return {
                makeBinary("*", std::move(right.coefficient), cloneExpr(left.remainder.get())),
                makeBinary("*", std::move(left.remainder), std::move(right.remainder)),
                true
            };
        }
        if(binary->op == "/" && left.containsTarget) {
            return {
                makeBinary("/", std::move(left.coefficient), cloneExpr(right.remainder.get())),
                makeBinary("/", std::move(left.remainder), std::move(right.remainder)),
                true
            };
        }
        if(binary->op == "/" && right.containsTarget) {
            throw SemanticError("The unknown cannot be used as a divisor.");
        }
    }

    throw SemanticError("Unsupported operator in linear equation: " + binary->op);
}

std::unique_ptr<ExprNode> solveLinearEquation(const EquationAST& equation) {
    LinearForm left = linearize(equation.left.get(), equation.varneedtofind);
    LinearForm right = linearize(equation.right.get(), equation.varneedtofind);

    if(!left.containsTarget && !right.containsTarget) {
        throw SemanticError("Equation does not contain unknown variable: " + equation.varneedtofind);
    }

    auto coefficient = makeBinary("-", std::move(left.coefficient), std::move(right.coefficient));
    auto remainder = makeBinary("-", std::move(right.remainder), std::move(left.remainder));
    return makeBinary("/", std::move(remainder), std::move(coefficient));
}
}

void SemanticAnalyzer::enterScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::leaveScope() {
    if(scopes_.empty()) {
        throw SemanticError("No active scope to leave.");
    }
    scopes_.pop_back();
}

void SemanticAnalyzer::declareVariable(const std::string& name, const std::string& typeName, bool isArray) {
    if(scopes_.empty()) {
        throw SemanticError("No active scope for variable declaration: " + name);
    }
    if(scopes_.back().count(name) != 0) {
        throw SemanticError("Redefinition of variable: " + name);
    }
    scopes_.back()[name] = {typeName, isArray};
}

const SemanticAnalyzer::VariableInfo* SemanticAnalyzer::lookupVariable(const std::string& name) const {
    for(auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if(found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

void SemanticAnalyzer::declareFunction(const std::string& name, const std::string& returnType, std::vector<std::string> paramTypes) {
    if(functions_.count(name) != 0) {
        throw SemanticError("Redefinition of function: " + name);
    }
    functions_[name] = {returnType, std::move(paramTypes)};
}

const SemanticAnalyzer::FunctionInfo* SemanticAnalyzer::lookupFunction(const std::string& name) const {
    auto it = functions_.find(name);
    if(it != functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SemanticAnalyzer::isNumericType(const std::string& typeName) const {
    return typeName == "int" || typeName == "float";
}

bool SemanticAnalyzer::isAssignable(const ExprNode* expr) const {
    return dynamic_cast<const VariableExprAST*>(expr) != nullptr ||
           dynamic_cast<const StructAccessAST*>(expr) != nullptr ||
           dynamic_cast<const ArrayAccessAST*>(expr) != nullptr;
}

bool SemanticAnalyzer::isAssignmentCompatible(const std::string& targetType, const std::string& valueType) const {
    return targetType == valueType || (targetType == "float" && valueType == "int");
}

void SemanticAnalyzer::ensureCompatibleTypes(const std::string& leftType, const std::string& rightType, const std::string& opName) const {
    if(leftType == rightType) {
        return;
    }

    if((leftType == "int" && rightType == "float") || (leftType == "float" && rightType == "int")) {
        return;
    }

    throw SemanticError("Type mismatch in operator '" + opName + "': " + leftType + " and " + rightType);
}

std::string SemanticAnalyzer::typeOf(const ExprNode* expr) const {
    if(!expr) {
        return "void";
    }

    if(auto* node = dynamic_cast<const NumberExprAST*>(expr)) {
        (void)node;
        return "int";
    }
    if(auto* node = dynamic_cast<const FloatExprAST*>(expr)) {
        (void)node;
        return "float";
    }
    if(auto* node = dynamic_cast<const VariableExprAST*>(expr)) {
        auto* info = lookupVariable(node->name);
        if (!info) {
            throw SemanticError("Unknown variable: " + node->name);
        }
        return info->typeName;
    }
    if(auto* node = dynamic_cast<const StructAccessAST*>(expr)) {
        auto* base = dynamic_cast<const VariableExprAST*>(node->base.get());
        if (!base) {
            throw SemanticError("Struct access target must be a variable.");
        }
        auto* var = lookupVariable(base->name);
        if (!var) {
            throw SemanticError("Unknown variable: " + base->name);
        }

        auto structInfo = structs_.find(var->typeName);
        if (structInfo == structs_.end()) {
            throw SemanticError("Type '" + var->typeName + "' is not a struct.");
        }
        auto fieldIt = structInfo->second.find(node->fieldName);
        if (fieldIt == structInfo->second.end()) {
            throw SemanticError("Unknown struct field '" + node->fieldName + "' in '" + var->typeName + "'.");
        }
        return fieldIt->second;
    }
    if(auto* node = dynamic_cast<const ArrayAccessAST*>(expr)) {
        auto* base = dynamic_cast<const VariableExprAST*>(node->base.get());
        if (!base) {
            throw SemanticError("Array access target must be a variable.");
        }
        auto* var = lookupVariable(base->name);
        if (!var) {
            throw SemanticError("Unknown variable: " + base->name);
        }
        if (!var->isArray) {
            throw SemanticError("Variable '" + base->name + "' is not an array.");
        }
        return var->typeName;
    }
    if(auto* node = dynamic_cast<const CallExprAST*>(expr)) {
        auto* fn = lookupFunction(node->Callee);
        if (!fn) {
            throw SemanticError("Unknown function: " + node->Callee);
        }
        if (node->Args.size() != fn->paramTypes.size()) {
            throw SemanticError("Function '" + node->Callee + "' expects " + std::to_string(fn->paramTypes.size()) + " arguments but got " + std::to_string(node->Args.size()) + ".");
        }
        for (size_t i = 0; i < node->Args.size(); ++i) {
            std::string actual = typeOf(node->Args[i].get());
            if (actual != fn->paramTypes[i]) {
                throw SemanticError("Argument type mismatch in call to '" + node->Callee + "' at parameter " + std::to_string(i + 1));
            }
        }
        return fn->returnType;
    }
    if(auto* node = dynamic_cast<const BinaryExprAST*>(expr)) {
        std::string lhs = typeOf(node->left.get());
        std::string rhs = typeOf(node->right.get());

        if(node->op == "=") {
            if (!isAssignable(node->left.get())) {
                throw SemanticError("Left-hand side of assignment is not assignable.");
            }
            if (!isAssignmentCompatible(lhs, rhs)) {
                throw SemanticError("Cannot assign " + rhs + " to " + lhs);
            }
            return lhs;
        }

        if(node->op == "+" || node->op == "-" || node->op == "*" || node->op == "/") {
            ensureCompatibleTypes(lhs, rhs, node->op);
            if (lhs == "float" || rhs == "float") {
                return "float";
            }
            return "int";
        }

        if(node->op == "<" || node->op == ">" || node->op == "==" || node->op == "!=" || node->op == "<=" || node->op == ">=") {
            ensureCompatibleTypes(lhs, rhs, node->op);
            return "int";
        }

        throw SemanticError("Unsupported binary operator: " + node->op);
    }
    if(auto* node = dynamic_cast<const UnaryMinusExprAST*>(expr)) {
        std::string operandType = typeOf(node->operand.get());
        if (!isNumericType(operandType)) {
            throw SemanticError("Unary '-' requires numeric operand.");
        }
        return operandType;
    }
    return "unknown";
}

std::string SemanticAnalyzer::typeOf(const ASTNode* node) const {
    if(!node) {
        return "void";
    }
    if(auto* expr = dynamic_cast<const ExprNode*>(node)) {
        return typeOf(expr);
    }
    if(auto* stmt = dynamic_cast<const ReturnStmtAST*>(node)) {
        if (!stmt->Value) {
            return "void";
        }
        return typeOf(static_cast<const ExprNode*>(stmt->Value.get()));
    }
    if(auto* var = dynamic_cast<const VarDecAST*>(node)) {
        return var->typeName;
    }
    return "void";
}

void SemanticAnalyzer::validateExpr(const ExprNode* expr) {
    if(!expr) {
        return;
    }
    (void)typeOf(expr);
}

void SemanticAnalyzer::validateBlock(const BlockAST& block) {
    enterScope();
    for(const auto& statement : block.Statements) {
        if(statement) {
            validateNode(statement.get());
        }
    }
    leaveScope();
}

void SemanticAnalyzer::validateFunction(const FunctionAST& function) {
    if(!function.Prototype || !function.Body) {
        return;
    }

    std::string returnType = "int";
    std::vector<std::string> params;
    for(const auto& arg : function.Prototype->Args) {
        params.push_back("int");
    }
    declareFunction(function.Prototype->Name, returnType, std::move(params));

    functionStack_.push_back(function.Prototype->Name);
    enterScope();
    for(size_t i = 0; i < function.Prototype->Args.size(); ++i) {
        declareVariable(function.Prototype->Args[i], "int");
    }

    validateNode(function.Body.get());

    leaveScope();
    functionStack_.pop_back();
}

void SemanticAnalyzer::validateStatement(const ASTNode* stmt) {
    if(!stmt) {
        return;
    }

    if(auto* var = dynamic_cast<const VarDecAST*>(stmt)) {
        if(var->typeName.empty()) {
            throw SemanticError("Variable declaration without type.");
        }
        declareVariable(var->name, var->typeName);
        if(var->initializer) {
            std::string initType = typeOf(var->initializer.get());
            if (!isAssignmentCompatible(var->typeName, initType)) {
                throw SemanticError("Cannot initialize " + var->typeName + " variable '" + var->name + "' with " + initType + ".");
            }
        }
        return;
    }

    if(auto* array = dynamic_cast<const VLADeclAST*>(stmt)) {
        if(typeOf(array->sizeExpr.get()) != "int") {
            throw SemanticError("Array size must have type int.");
        }
        declareVariable(array->arrayName, "int", true);
        return;
    }

    if(auto* ret = dynamic_cast<const ReturnStmtAST*>(stmt)) {
        if(functionStack_.empty()) {
            throw SemanticError("Return outside of function.");
        }
        if(ret->Value) {
            std::string valueType = typeOf(ret->Value.get());
            if(valueType != "int" && valueType != "float") {
                throw SemanticError("Return value must be numeric.");
            }
        }
        return;
    }

    if(auto* block = dynamic_cast<const BlockAST*>(stmt)) {
        validateBlock(*block);
        return;
    }

    if(auto* ifStmt = dynamic_cast<const IfStmtAST*>(stmt)) {
        validateExpr(ifStmt->Condition.get());
        if(ifStmt->Then) {
            validateNode(ifStmt->Then.get());
        }
        if(ifStmt->Else) {
            validateNode(ifStmt->Else.get());
        }
        return;
    }

    if(dynamic_cast<const BreakAST*>(stmt)) {
        if(!inLoop_) {
            throw SemanticError("'break' outside of loop.");
        }
        return;
    }

    if(dynamic_cast<const ContinueAST*>(stmt)) {
        if(!inLoop_) {
            throw SemanticError("'continue' outside of loop.");
        }
        return;
    }

    if(auto* loop = dynamic_cast<const WhileLoopAST*>(stmt)) {
        const bool previous = inLoop_;
        inLoop_ = true;
        try {
            if (auto* condExpr = dynamic_cast<const ExprNode*>(loop->cond.get())) {
                if (typeOf(condExpr) != "int") {
                    throw SemanticError("While condition must have type int.");
                }
            } else {
                throw SemanticError("While condition must be an expression.");
            }
            if (loop->body) {
                validateNode(loop->body.get());
            }
        } catch(...) {
            inLoop_ = previous;
            throw;
        }
        inLoop_ = previous;
        return;
    }

    if(auto* equation = dynamic_cast<const EquationAST*>(stmt)) {
        auto* target = lookupVariable(equation->varneedtofind);
        if(!target) {
            throw SemanticError("Unknown equation variable: " + equation->varneedtofind);
        }
        if(!isNumericType(target->typeName)) {
            throw SemanticError("Equation variable must be numeric: " + equation->varneedtofind);
        }

        std::string leftType = typeOf(equation->left.get());
        std::string rightType = typeOf(equation->right.get());

        if(!isNumericType(leftType) || !isNumericType(rightType)) {
            throw SemanticError("Equation sides must be a numeric type");
        }
        ensureCompatibleTypes(leftType, rightType, "equation");
        equation->solution = solveLinearEquation(*equation);

        std::string solutionType = typeOf(equation->solution.get());
        if(!isAssignmentCompatible(target->typeName, solutionType)) {
            throw SemanticError("Cannot assign equation result to " + equation->varneedtofind + ".");
        }
        return;
    }

    if(auto* call = dynamic_cast<const CallExprAST*>(stmt)) {
        validateExpr(call);
        return;
    }

    if(auto* expr = dynamic_cast<const ExprNode*>(stmt)) {
        validateExpr(expr);
        return;
    }

    if(auto* func = dynamic_cast<const FunctionAST*>(stmt)) {
        validateFunction(*func);
        return;
    }

    if(auto* structDecl = dynamic_cast<const StructDeclAST*>(stmt)) {
        if (structs_.count(structDecl->structName) != 0) {
            throw SemanticError("Redefinition of struct: " + structDecl->structName);
        }
        std::unordered_map<std::string, std::string> fields;
        for (const auto& field : structDecl->fields) {
            fields[field.fieldName] = field.typeName;
        }
        structs_[structDecl->structName] = std::move(fields);
        return;
    }

    throw SemanticError("Unsupported statement node in semantic analysis.");
}

void SemanticAnalyzer::validateNode(const ASTNode* node) {
    validateStatement(node);
}

void SemanticAnalyzer::analyze(const std::vector<std::unique_ptr<ASTNode>>& statements) {
    enterScope();
    for(const auto& stmt : statements) {
        if(stmt) {
            validateNode(stmt.get());
        }
    }
    leaveScope();
}
