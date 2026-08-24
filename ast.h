#pragma once
#include <iostream>
#include <string>
#include <memory>

//Base class for all tree nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
};

class ExprNode : public ASTNode {};

class StmtNode : public ASTNode {}; //Base class for instuctions

class NumberExprAST : public ExprNode {
public:
    int value;
    explicit NumberExprAST(int val) : value(val) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Number(" << value << ")\n";
    }
};

class BinaryExprAST : public ExprNode {
public:
    char op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;

    BinaryExprAST(char op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) : op(op), left(std::move(left)), right(std::move(right)) {}
    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "BinaryOp(" << op << ")\n";
        if(left) left->print(indent+1);
        if(right) right->print(indent+1);
    }
};

class VariableExprAST : public ExprNode {
public:
    std::string name;

    explicit VariableExprAST(std::string name) : name(std::move(name)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Variable(" << name << ")\n";
    }
};

class VarDecAST : public StmtNode {
public:
    std::string name;
    std::unique_ptr<ExprNode> initializer;

    VarDecAST(std::string name, std::unique_ptr<ExprNode> init) : name(std::move(name)), initializer(std::move(init)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "VarDecl(" << name << ")\n";
        if(initializer) initializer->print(indent+1);
    }
};