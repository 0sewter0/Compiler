#pragma once
#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/ADT/APInt.h"

#include "SymbTable.h"

extern llvm::LLVMContext Context;
extern llvm::IRBuilder<> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
extern SymbolTable symbolTable;

struct LoopBlocks {
    llvm::BasicBlock* CondBB; // For continue
    llvm::BasicBlock* AfterBB; // For break
};

struct FieldDef {
    std::string typeName;
    std::string fieldName;
};

struct Vector {
    int* data;
    int size;
    int capasity;
};

extern std::vector<LoopBlocks> LoopStack;

enum class ASTNodeType {
    Program,
    VariableExpr,
    StructDecl,
    StructAccess,
    StructAssign,
    ArrayAccess,
    ArrayDecl,
    ArrayAssign,
    VLADecl,
    Break,
    Continue,
    CallExpr,
    Block,
    WhileLoop,
    Prototype,
    Function,
    ReturnStmt,
    IfStmt,
    NumberExpr,
    FloatExpr,
    BinaryExpr,
    UnaryMinusExpr,
    VarDecl,
    Error,
    Equation
};

template<typename To, typename From>
bool isa(const From* Val) {
    if(!Val) return false;
    return To::classof(Val);
}

template<typename To, typename From>
To* cast(From* Val) {
    return static_cast<To*>(Val);
}

template<typename To, typename From>
To* cast_or_null(From* Val) {
    return isa<To>(Val) ? static_cast<To*>(Val) : nullptr;
}

//Base class
class ASTNode {
private:
    ASTNodeType Type;
public:
    ASTNode() = default;
    ASTNode(ASTNodeType t) : Type(t) {}
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0; // For printing AST
    virtual llvm::Value* codegen() = 0; // Generating IR.

    ASTNodeType getType() const { return Type; }
};

class ExprNode : public ASTNode {
public:
    ExprNode() = default;
    ExprNode(ASTNodeType t) : ASTNode(t) {}

    static bool classof(const ASTNode* Box) {
        auto T = Box->getType();
        return T == ASTNodeType::ArrayAccess ||
        T == ASTNodeType::ArrayAssign ||
        T == ASTNodeType::StructAccess ||
        T == ASTNodeType::StructAssign ||
        T == ASTNodeType::BinaryExpr ||
        T == ASTNodeType::CallExpr ||
        T == ASTNodeType::NumberExpr ||
        T == ASTNodeType::FloatExpr ||
        T == ASTNodeType::UnaryMinusExpr ||
        T == ASTNodeType::VariableExpr;
    }

    // Returns the address of an assignable expression, or nullptr when this
    // expression is a value only (for example, a number or a binary sum).
    virtual llvm::Value* codegenAddress() {
        return nullptr;
    }
};

class VariableExprAST : public ExprNode { 
public:
    std::string name;
    explicit VariableExprAST(std::string name) : ExprNode(ASTNodeType::VariableExpr), name(std::move(name)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::VariableExpr;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Variable(" << name << ")\n";
    }

    llvm::Value* codegenAddress() override {
        llvm::AllocaInst* Alloca = symbolTable.lookupVariable(name).Alloca;
        if(!Alloca) {
            std::cerr << "Unknown Variable name: " << name << std::endl;
            return nullptr;
        }
        return Alloca;
    }

    llvm::Value* codegen() override;
};

/*------------------------------------------------------------------------------------------------------*/

class StructDeclAST : public ASTNode {
public:
    std::string structName;
    std::vector<FieldDef> fields;

    StructDeclAST(std::string Name, std::vector<FieldDef> Fields) : ASTNode(ASTNodeType::StructDecl), structName(std::move(Name)), fields(std::move(Fields)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::StructDecl;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Struct Declarartion(name: " << structName;
    }
};

class StructAccessAST : public ExprNode {
public:
    std::unique_ptr<ExprNode> base;
    std::string fieldName;

    StructAccessAST(std::unique_ptr<ExprNode> Base, std::string FieldName) : ExprNode(ASTNodeType::StructAccess), base(std::move(Base)), fieldName(std::move(FieldName)) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::StructAccess;
    }

    llvm::Value* codegen() override;
    llvm::Value* codegenAddress() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << "StructAccess( " << "fieldName: " << fieldName << ")\n";
    }
};

class StructAssignAST : public ExprNode {
public:
    std::string varName;
    std::string fieldName;
    std::unique_ptr<ExprNode> value;

    StructAssignAST(std::string VarName, std::string FieldName, std::unique_ptr<ExprNode> Value) : ExprNode(ASTNodeType::StructAssign), varName(std::move(VarName)), fieldName(std::move(FieldName)), value(std::move(Value)) {}

    static bool classof(ASTNode* Box) {
        return Box->getType() == ASTNodeType::StructAssign;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "WriteIntoField(varName: " << varName << ", fieldName: " << fieldName << ", val: " << value << ")\n";
    } 
};

/*-------------------------------------------*/

class ArrayAccessAST : public ExprNode {
public:
    std::unique_ptr<ExprNode> base;
    std::unique_ptr<ASTNode> index;

    ArrayAccessAST(std::unique_ptr<ExprNode> Base, std::unique_ptr<ExprNode> Index) : ExprNode(ASTNodeType::ArrayAccess), base(std::move(Base)), index(std::move(Index)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::ArrayAccess;
    }

    llvm::Value* codegen() override;
    llvm::Value* codegenAddress() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ArrayAccess(index: " << index << ")\n";
    }
};

class ArrayAssignAST : public ExprNode {
public:
    std::unique_ptr<ExprNode> index;
    std::string name;
    std::unique_ptr<ExprNode> value;


    ArrayAssignAST(std::unique_ptr<ExprNode> Index, std::unique_ptr<ExprNode> Value, std::string &Name) : ExprNode(ASTNodeType::ArrayAssign), index(std::move(Index)), value(std::move(Value)), name(Name) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::ArrayAssign;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "WriteintoArray(name: " << name << ", index: " << index << ", val: " << value << ")\n"; 
    }
};

class VLADeclAST : public ASTNode {
public:
    std::string arrayName;
    std::unique_ptr<ExprNode> sizeExpr;

    VLADeclAST(std::string ArrayName, std::unique_ptr<ExprNode> SizeExpr) : ASTNode(ASTNodeType::VLADecl), arrayName(ArrayName), sizeExpr(std::move(SizeExpr)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::VLADecl;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "DynamicArrayDecl(name: " << arrayName << ", sizeExpr: \n";
        sizeExpr->print();
    }
};

/*---------------------------------------------------------------------------------------------------------------------*/

class BreakAST : public ASTNode {
public:
    BreakAST() : ASTNode(ASTNodeType::Break) {}

    llvm::Value* codegen() override;

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Break;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Break\n";
    }
};

class ContinueAST : public ASTNode {
public:
    ContinueAST() : ASTNode(ASTNodeType::Continue) {}

    llvm::Value* codegen() override;

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Continue;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Continue\n";
    }
};

class WhileLoopAST : public ASTNode {
public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> body;
    
    WhileLoopAST(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> body) : ASTNode(ASTNodeType::WhileLoop), cond(std::move(cond)), body(std::move(body)) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::WhileLoop;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "WhileLoop\n";

        if(cond) cond->print();
        if(body) body->print();
    }
};

/*---------------------------------------------------------------------------------------------------------*/

class BlockAST : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> Statements;

    BlockAST(std::vector<std::unique_ptr<ASTNode>> Stmts) : ASTNode(ASTNodeType::Block), Statements(std::move(Stmts)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Block;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "BlockStatement {\n";
        
        for (const auto& Stmt : Statements) {
            if (Stmt) Stmt->print(indent + 1);
        }
        std::cout << space << "}\n";
    }
};

/*----------------------------------------------------------*/

class CallExprAST : public ExprNode {
public:
    std::string Callee;
    std::vector<std::unique_ptr<ExprNode>> Args;

    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprNode>> Args) : ExprNode(ASTNodeType::CallExpr), Callee(Callee), Args(std::move(Args)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::CallExpr;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "CallExpr:" << Callee;

        for(const auto& arg : Args) {
            if(arg) arg->print(indent+1);
        }
    }
};

class PrototypeAST : public ASTNode {
public:
    std::string Name;
    std::vector<std::string> Args;

    PrototypeAST(const std::string &Name, std::vector<std::string> Args) : ASTNode(ASTNodeType::Prototype), Name(Name), Args(std::move(Args)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Prototype;
    }

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "FPrototype\n";
    }

    llvm::Value* codegen() override;
};

class FunctionAST : public ASTNode {
public:
    std::unique_ptr<PrototypeAST> Prototype;
    std::unique_ptr<ASTNode> Body;
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ASTNode> Body) : ASTNode(ASTNodeType::Function), Prototype(std::move(Proto)), Body(std::move(Body)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Function;
    }

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Function body: \n";
        Body->print();
    }

    llvm::Value* codegen() override;
};

class ReturnStmtAST : public ASTNode {
public:
    std::unique_ptr<ASTNode> Value;
    ReturnStmtAST(std::unique_ptr<ASTNode> Val) : ASTNode(ASTNodeType::ReturnStmt), Value(std::move(Val)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::ReturnStmt;
    }

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ReturnStmt(Val: " << Value << ")\n";
    }

    llvm::Value* codegen() override;
};

/*-----------------------------------------------------------------------------------------------------------*/

class StmtNode : public ASTNode {};

class IfStmtAST : public ASTNode {
public:
    std::unique_ptr<ExprNode> Condition;
    std::unique_ptr<ASTNode> Then;
    std::unique_ptr<ASTNode> Else;

    IfStmtAST(std::unique_ptr<ExprNode> Cond, std::unique_ptr<ASTNode> Then, std::unique_ptr<ASTNode> Else) : ASTNode(ASTNodeType::IfStmt), Condition(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::IfStmt;
    }
    
    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "IfStmt\n";
        if(Condition) Condition->print(indent+1);
        if(Then) Then->print(indent+1);
        if(Else) Else->print(indent+1);
    }
    llvm::Value* codegen() override;
};

/*--------------------------------------------------------------------------------------------------------------*/

class NumberExprAST : public ExprNode {
public:
    int value;
    explicit NumberExprAST(int val) : ExprNode(ASTNodeType::NumberExpr), value(val) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::NumberExpr;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Number(" << value << ")\n";
    }

    llvm::Value* codegen() override;
};

class FloatExprAST : public ExprNode {
    float val;
public:
    FloatExprAST(float Val) : ExprNode(ASTNodeType::FloatExpr), val(Val) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::FloatExpr;
    }

    float value() const { return val; }
    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << "Float Expression, value: " << val;
    }
};

class BinaryExprAST : public ExprNode {
public:
    std::string op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;

    BinaryExprAST(std::string Op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) : ExprNode(ASTNodeType::BinaryExpr), op(Op), left(std::move(left)), right(std::move(right)) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::BinaryExpr;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "BinaryOp(" << op << ")\n";
        if(left) left->print(indent+1);
        if(right) right->print(indent+1);
    }
    
    llvm::Value* codegen() override;

};

class UnaryMinusExprAST : public ExprNode {
public:
    char op;
    std::unique_ptr<ExprNode> operand;
    UnaryMinusExprAST(char Op, std::unique_ptr<ExprNode> Operand) : ExprNode(ASTNodeType::UnaryMinusExpr), op(Op), operand(std::move(Operand)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::UnaryMinusExpr;
    }

    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Unary Expression(op: " << op << ", operand: " << operand;
    }
};

/*--------------------------------------------------------------------------------------------------------------*/

class VarDecAST : public ASTNode { // For varibale declaration
public:
    std::string name;
    std::string typeName;
    std::unique_ptr<ExprNode> initializer;

    VarDecAST(std::string name, std::unique_ptr<ExprNode> init, std::string TypeName) : ASTNode(ASTNodeType::VarDecl), name(std::move(name)), initializer(std::move(init)), typeName(std::move(TypeName)) {}
    ~VarDecAST() override;

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::VarDecl;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "VarDecl(" << name << ")\n";
        if(initializer) initializer->print(indent+1);
    }
    llvm::Value* codegen() override;
};

class ArrayDeclAST : public ASTNode {
public:
    std::string name;
    std::string typeName;
    std::unique_ptr<ExprNode> sizeExpr;

    ArrayDeclAST(std::string Name, std::unique_ptr<ExprNode> SizeExpr, std::string TypeName = "int") : ASTNode(ASTNodeType::ArrayDecl), name(std::move(Name)), sizeExpr(std::move(SizeExpr)), typeName(std::move(TypeName)) {}
    
    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::ArrayDecl;
    }

    ~ArrayDeclAST() override;

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "ArrayDecl(" << name << ")\n";
        if(sizeExpr) sizeExpr->print(indent + 1);
    }

    llvm::Value* codegen() override;
};

/*------------------------------------------------------------------------------------------------------*/

class EquationAST : public ASTNode {
public:
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
    std::string varneedtofind;
    mutable std::unique_ptr<ExprNode> solution;

    EquationAST(std::unique_ptr<ExprNode> Left, std::unique_ptr<ExprNode> Right, std::string var) : ASTNode(ASTNodeType::Equation), left(std::move(Left)), right(std::move(Right)), varneedtofind(var) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Equation;
    }

    llvm::Value* codegen() override;
    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Equation AST\n";
    }
};

/*---------------------------------------------------------------------------------------------------*/

class ErrorExprNode : public ExprNode {
public:
    ErrorExprNode() : ExprNode(ASTNodeType::Error) {}

    llvm::Value* codegen() override {
        return nullptr;
    }
    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ErrorExpr\n";
    }
};

class ProgramAST : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;

    explicit ProgramAST(std::vector<std::unique_ptr<ASTNode>> stmts) : ASTNode(ASTNodeType::Program), statements(std::move(stmts)) {}

    static bool classof(const ASTNode* Box) {
        return Box->getType() == ASTNodeType::Program;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Program\n";
        for(const auto& stmts : statements) {
            if(stmts) stmts->print(indent+1);
        }
    }

    llvm::Value* codegen() override;
};
