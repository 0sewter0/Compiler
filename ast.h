#pragma once
#include <iostream>
#include <string>
#include <map>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
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

extern std::vector<LoopBlocks> LoopStack;

inline llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* TheFunction, const std::string& VarName) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(llvm::Type::getInt32Ty(Context), nullptr, VarName);
}

//Base class for all nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0; // For printing AST
    virtual llvm::Value* codegen() = 0; // Generating IR.
};

class ExprNode : public ASTNode {};

class ArrayAccessAST : public ExprNode {
public:
    std::string name;
    std::unique_ptr<ASTNode> index;

    ArrayAccessAST(const std::string &Name, std::unique_ptr<ExprNode> Index) : name(Name), index(std::move(Index)) {}

    llvm::Value* codegen() override {
        llvm::Value* ArrayPtr = symbolTable.lookupVariable(name);
        if(!ArrayPtr) {
            std::cerr << "Unknown variable name: " << name << std::endl;
            return nullptr;
        }

        llvm::Value* IndexVal = index->codegen();
        if(!IndexVal) return nullptr;

        llvm::Value* IdxList[] = {Builder.getInt32(0), IndexVal};

        llvm::ArrayType* ArrayTy = llvm::ArrayType::get(Builder.getInt32Ty(), 5);

        llvm::Value* ElementPtr = Builder.CreateGEP(ArrayTy, ArrayPtr, IdxList, "arrayidx");

        return Builder.CreateLoad(Builder.getInt32Ty(), ElementPtr, "tmpld");
    }
    
    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ArrayAccess(name: " << name << ", index: " << index << ")\n";
    }
};

class ArrayDeclAST : public ASTNode {
public:
    std::string name;
    int size;

    ArrayDeclAST(std::string Name, int Size) : name(Name), size(Size) {}

    llvm::Value* codegen() override {
        llvm::ArrayType* ArrayTy = llvm::ArrayType::get(Builder.getInt32Ty(), size);

        llvm::AllocaInst* Alloca = Builder.CreateAlloca(ArrayTy, nullptr, name);

        uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(ArrayTy);
        llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);

        Builder.CreateLifetimeStart(Alloca, sizeVal);

        symbolTable.declareVariable(name, Alloca);

        return Alloca;
    }
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
    
    llvm::Value* codegen() {
        llvm::Value* arrayPtr = symbolTable.lookupVariable(name);
        if(!arrayPtr) return nullptr;

        llvm::Value* idxVal = index->codegen();
        llvm::Value* valToStore = value->codegen();

        llvm::Value* idxList[] = {Builder.getInt32(0), idxVal};
        llvm::ArrayType* arrayTy = llvm::ArrayType::get(Builder.getInt32Ty(), 5);

        llvm::Value* elemPtr = Builder.CreateGEP(arrayTy, arrayPtr, idxList, "arrayidx");

        return Builder.CreateStore(valToStore, elemPtr);
    }

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "WriteintoArray(name: " << name << ", index: " << index << ", val: " << value << ")\n"; 
    }
};

class BreakAST : public ASTNode {
public:
    llvm::Value* codegen() override {
        if (LoopStack.empty()) {
            // Compilation error: break is out of loop
            return nullptr;
        }
        
        llvm::BasicBlock* AfterBB = LoopStack.back().AfterBB;
        Builder.CreateBr(AfterBB);
        
        // LLVM IR does not allow code to be written after a jump instruction(terminator).
        // Therefore, you need to create a temporary "dead" block so that any subsequent code
        // does not break generation, or we return the result.
        llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();

        llvm::BasicBlock *DeadBB = llvm::BasicBlock::Create(Builder.getContext(), "dead", TheFunction);
        Builder.SetInsertPoint(DeadBB);
        
        return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Builder.getContext()));
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Break\n";
    }
};

class ContinueAST : public ASTNode {
public:
    llvm::Value* codegen() override {
        if (LoopStack.empty()) {
            // Compilation error: continue is out of loop
            return nullptr;
        }
        
        llvm::BasicBlock* CondBB = LoopStack.back().CondBB;
        
        Builder.CreateBr(CondBB);
        
        llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();

        llvm::BasicBlock *DeadBB = llvm::BasicBlock::Create(Builder.getContext(), "dead", TheFunction);
        Builder.SetInsertPoint(DeadBB);
        
        return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Builder.getContext()));
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
    
    WhileLoopAST(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> body) : cond(std::move(cond)), body(std::move(body)) {}
    llvm::Value* codegen() override {
        llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* CondBB = llvm::BasicBlock::Create(Context, "whilecond", TheFunction);
        llvm::BasicBlock* BodyBB = llvm::BasicBlock::Create(Context, "whileloop", TheFunction); // Creating 3 basic blocks.
        llvm::BasicBlock* AfterBB = llvm::BasicBlock::Create(Context, "whileafter", TheFunction);

        Builder.CreateBr(CondBB);

        // Condition block
        Builder.SetInsertPoint(CondBB);
        llvm::Value* CondV = cond->codegen();
        if(!CondV) return nullptr;
        Builder.CreateCondBr(CondV, BodyBB, AfterBB); // If cond is true jump into Body, else jump into After.

       // Body block
        TheFunction->insert(TheFunction->end(), BodyBB);
        Builder.SetInsertPoint(BodyBB);

        LoopStack.push_back({CondBB, AfterBB});

        llvm::Value* BodyV = body->codegen();

        LoopStack.pop_back();

        if(!Builder.GetInsertBlock()->getTerminator()) {
            Builder.CreateBr(CondBB);
        }

        // Exit block
        TheFunction->insert(TheFunction->end(), AfterBB);
        Builder.SetInsertPoint(AfterBB);

        return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Context)); // Basicly, loops return zero/void value
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "WhileLoop\n";

        if(cond) cond->print();
        if(body) body->print();
    }
};

class BlockAST : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> Statements;

    BlockAST(std::vector<std::unique_ptr<ASTNode>> Stmts) : Statements(std::move(Stmts)) {}

    llvm::Value* codegen() override {
        // New block
        symbolTable.pushScope();

        //  generating code for all instructions in block
        for (auto &Stmt : Statements) {
            Stmt->codegen();
        }

        auto LocalVariables = symbolTable.popScope();

        for(auto const &[name, allocaInst] : LocalVariables) {
            if(allocaInst) {
            // Gain type size in bytes

                if(Builder.GetInsertBlock() && Builder.GetInsertBlock()->getTerminator()) {
                    break;
                }
                llvm::Type* varType = allocaInst->getAllocatedType();
                uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(varType);
                llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);


                Builder.CreateLifetimeEnd(allocaInst, sizeVal);
            }
        }

        return nullptr;
    }

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "BlockStatement {\n";
        
        for (const auto& Stmt : Statements) {
            if (Stmt) Stmt->print(indent + 1);
        }
        std::cout << space << "}\n";
    }
};

class CallExprAST : public ExprNode {
public:
    std::string Callee;
    std::vector<std::unique_ptr<ExprNode>> Args;

    CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprNode>> Args) : Callee(Callee), Args(std::move(Args)) {}

    llvm::Value* codegen() override {
        llvm::Function* Calleef = TheModule->getFunction(Callee);
        if(!Calleef) { std::cout << "Unknown function referenced"; return nullptr; }

        std::vector<llvm::Value*> ArgsV;
        for(auto& arg : Args) {
            ArgsV.push_back(arg->codegen());
        }
        return Builder.CreateCall(Calleef, ArgsV, "calltmp");
    }

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

    llvm::Value* codegen() override {
        std::vector<llvm::Type*> Ints(Args.size(), llvm::Type::getInt32Ty(Context));
        llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), Ints, false);
        llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

        unsigned Idx = 0;
        for(auto &Arg : F->args()) Arg.setName(Args[Idx++]);
        return F;
    } 
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

    llvm::Value* codegen() override {
        llvm::Function *TheFunction =
        TheModule->getFunction(Prototype->Name);

        if(!TheFunction) {
            Prototype->codegen();
            TheFunction = TheModule->getFunction(Prototype->Name);
        }

        if(!TheFunction) return nullptr;

        if(!TheFunction->empty()) {
            std::cerr << "Compilation error: Function cannot be redefined\n";
            return nullptr;
        }

        llvm::BasicBlock *BB = llvm::BasicBlock::Create(Context, "entry", TheFunction);
        Builder.SetInsertPoint(BB);

        symbolTable.pushScope();

        for(auto &Arg : TheFunction->args()) {
            llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
            llvm::AllocaInst *Alloca = TmpB.CreateAlloca(llvm::Type::getInt32Ty(Context), nullptr, std::string(Arg.getName()));

            Builder.CreateStore(&Arg, Alloca);

            symbolTable.declareVariable(
            std::string(Arg.getName()), Alloca);
        } 

        Body->codegen();

        /*llvm::BasicBlock* curBB = Builder.GetInsertBlock();
        if(curBB && !curBB->getTerminator()) {
            Builder.CreateRet(Builder.getInt32(0));
        } */
        symbolTable.popScope();
        return TheFunction;
    }
};

class ReturnStmtAST : public ASTNode {
public:
    std::unique_ptr<ASTNode> Value;
    ReturnStmtAST(std::unique_ptr<ASTNode> Val) : Value(std::move(Val)) {}

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "ReturnStmt(Val: " << Value << ")\n";
    }

    llvm::Value* codegen() override {
        llvm::Value* RetVal = Value ? Value->codegen() : Builder.getInt32(0);
        return Builder.CreateRet(RetVal);
    }
};

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
    llvm::Value* codegen() override {
        llvm::Value* CondV = Condition->codegen();
        if(!CondV) return nullptr;

        CondV = Builder.CreateICmpNE(CondV, llvm::ConstantInt::get(Context, llvm::APInt(32, 0)), "ifcond");

        llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(Context, "then", TheFunction);
        llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(Context, "else");
        llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(Context, "ifcont");

        Builder.CreateCondBr(CondV, ThenBB, ElseBB);

        Builder.SetInsertPoint(ThenBB);
        Then->codegen();

        ThenBB = Builder.GetInsertBlock();

        if(!ThenBB->getTerminator()) {
            Builder.CreateBr(MergeBB);
        }

        TheFunction->insert(TheFunction->end(), ElseBB);
        Builder.SetInsertPoint(ElseBB);

        if(Else) {
            Else->codegen();
        }

        ElseBB = Builder.GetInsertBlock();
        if(ElseBB->getTerminator()) {
            Builder.CreateBr(MergeBB);
        }

        TheFunction->insert(TheFunction->end(), MergeBB);
        Builder.SetInsertPoint(MergeBB);

        return nullptr;
    }
};

class NumberExprAST : public ExprNode {
public:
    int value;
    explicit NumberExprAST(int val) : value(val) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Number(" << value << ")\n";
    }

    llvm::Value* codegen() override {
        return llvm::ConstantInt::get(Context, llvm::APInt(32, value, true));
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
    
    llvm::Value* codegen() override {
        llvm::Value* L = left->codegen();
        llvm::Value* R = right->codegen();
        if(!L || !R) {
            return nullptr;
        }

        switch (op) {
            case '+': return Builder.CreateAdd(L, R, "addtmp"); break;
            case '-': return Builder.CreateSub(L, R, "subtmp"); break; 
            case '*': return Builder.CreateMul(L, R, "multmp"); break;
            case '/': return Builder.CreateSDiv(L, R, "divtmp"); break;
            case '>': return Builder.CreateICmpSGT(L, R, "cmptmp"); break;
            case '<': return Builder.CreateICmpSLT(L, R, "cmptmp"); break;
            case '&': return Builder.CreateAnd(L, R, "andtmp"); break;
            default:
                std::cerr << "Unknown binary operation" << op << std::endl;
                return nullptr;
        }
    }
};

class VariableExprAST : public ExprNode { // For expressions where a variable is used
public:
    std::string name;

    explicit VariableExprAST(std::string name) : name(std::move(name)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Variable(" << name << ")\n";
    }

    llvm::Value* codegen() override {
        llvm::AllocaInst* A = symbolTable.lookupVariable(name);
        if(!A) {
            std::cerr << "Unknown Variable name: " << name << std::endl;
            return nullptr;
        }
        return Builder.CreateLoad(A->getAllocatedType(), A, name.c_str());
    }
};

class VarDecAST : public ASTNode { // For varibale declaration
public:
    std::string name;
    std::unique_ptr<ExprNode> initializer;

    VarDecAST(std::string name, std::unique_ptr<ExprNode> init) : name(std::move(name)), initializer(std::move(init)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "VarDecl(" << name << ")\n";
        if(initializer) initializer->print(indent+1);
    }

    llvm::Value* codegen() override {
        llvm::Value* InitVal = initializer->codegen();
        if(!InitVal) return nullptr;

        llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();
        llvm::AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, name);
        
        Builder.CreateStore(InitVal, Alloca);

        uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(Builder.getInt32Ty());
        llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);
        Builder.CreateLifetimeStart(Alloca, sizeVal);
        symbolTable.declareVariable(name, Alloca);


        return InitVal;
    }
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

    llvm::Value* codegen() override {
        llvm::Value* LastVal = nullptr;
        for(auto& stmts : statements) {
            if(stmts) LastVal = stmts->codegen();
        }
        return LastVal; // Returns last instruction result
    }
};