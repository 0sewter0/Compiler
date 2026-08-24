#pragma once
#include <string>
#include <memory>

//Base class for all tree nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
};

class ExprNode : public ASTNode {};

class StmtNode : public ASTNode {}; //Base class for instuctions

class NumberExprAST : public ExprNode {
public:
    int value;
    explicit NumberExprAST(int val) : value(val) {}
};

class BinaryExprAST : public ExprNode {
public:
    char op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;

    BinaryExprAST(char op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) : op(op), left(std::move(left)), right(std::move(right)) {}
};

class VarDecAST : public StmtNode {
    std::string name;
    std::unique_ptr<ExprNode> initializer;

    VarDecAST(std::string name, std::unique_ptr<ExprNode> init) : name(std::move(name)), initializer(std::move(init)) {}
};