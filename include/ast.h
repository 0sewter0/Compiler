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

//Base class for all nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0; // For printing AST
    virtual llvm::Value* codegen() = 0; // Generating IR.
};

class ExprNode : public ASTNode {
public:
    // Returns the address of an assignable expression, or nullptr when this
    // expression is a value only (for example, a number or a binary sum).
    virtual llvm::Value* codegenAddress() {
        return nullptr;
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

class StructDeclAST : public ASTNode {
public:
    std::string structName;
    std::vector<FieldDef> fields;

    StructDeclAST(std::string Name, std::vector<FieldDef> Fields) : structName(std::move(Name)), fields(std::move(Fields)) {}

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

    StructAccessAST(std::unique_ptr<ExprNode> Base, std::string FieldName) : base(std::move(Base)), fieldName(std::move(FieldName)) {}
    
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

    StructAssignAST(std::string VarName, std::string FieldName, std::unique_ptr<ExprNode> Value) : varName(std::move(VarName)), fieldName(std::move(FieldName)), value(std::move(Value)) {}

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

    ArrayAccessAST(std::unique_ptr<ExprNode> Base, std::unique_ptr<ExprNode> Index) : base(std::move(Base)), index(std::move(Index)) {}

    llvm::Value* codegen() override;
    llvm::Value* codegenAddress() override;

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ArrayAccess(index: " << index << ")\n";
    }
};

class ArrayDeclAST : public ASTNode {
public:
    std::string name;
    int size;

    ArrayDeclAST(std::string Name, int Size) : name(Name), size(Size) {}

    llvm::Value* codegen() override;
    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Array Declaration(name: " << name << ", size: " << size << ")\n";
    }
};

class ArrayAssignAST : public ExprNode {
public:
    std::unique_ptr<ExprNode> index;
    std::string name;
    std::unique_ptr<ExprNode> value;


    ArrayAssignAST(std::unique_ptr<ExprNode> Index, std::unique_ptr<ExprNode> Value, std::string &Name) : index(std::move(Index)), value(std::move(Value)), name(Name) {}
    
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

    VLADeclAST(std::string ArrayName, std::unique_ptr<ExprNode> SizeExpr) : arrayName(ArrayName), sizeExpr(std::move(SizeExpr)) {}

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
    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Break\n";
    }
};

class ContinueAST : public ASTNode {
public:
    llvm::Value* codegen() override;

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Continue\n";
    }
};

class WhileLoopAST : public ASTNode {
public:
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> body;
    
    WhileLoopAST(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> body) : cond(std::move(cond)), body(std::move(body)) {}
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

    BlockAST(std::vector<std::unique_ptr<ASTNode>> Stmts) : Statements(std::move(Stmts)) {}

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

    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprNode>> Args) : Callee(Callee), Args(std::move(Args)) {}

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

    PrototypeAST(const std::string &Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}

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
    FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<ASTNode> Body) : Prototype(std::move(Proto)), Body(std::move(Body)) {}

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
    ReturnStmtAST(std::unique_ptr<ASTNode> Val) : Value(std::move(Val)) {}

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

    IfStmtAST(std::unique_ptr<ExprNode> Cond, std::unique_ptr<ASTNode> Then, std::unique_ptr<ASTNode> Else) : Condition(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
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
    explicit NumberExprAST(int val) : value(val) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Number(" << value << ")\n";
    }

    llvm::Value* codegen() override;
};

class FloatExprAST : public ExprNode {
    float val;
public:
    FloatExprAST(float Val) : val(Val) {}
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

    BinaryExprAST(std::string Op, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) : op(Op), left(std::move(left)), right(std::move(right)) {}
    
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
    UnaryMinusExprAST(char Op, std::unique_ptr<ExprNode> Operand) : op(Op), operand(std::move(Operand)) {}

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

    VarDecAST(std::string name, std::unique_ptr<ExprNode> init, std::string TypeName) : name(std::move(name)), initializer(std::move(init)), typeName(std::move(TypeName)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "VarDecl(" << name << ")\n";
        if(initializer) initializer->print(indent+1);
    }

    llvm::Value* codegen() override;
};

class ProgramAST : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;

    explicit ProgramAST(std::vector<std::unique_ptr<ASTNode>> stmts) : statements(std::move(stmts)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Program\n";
        for(const auto& stmts : statements) {
            if(stmts) stmts->print(indent+1);
        }
    }

    llvm::Value* codegen() override;
};
