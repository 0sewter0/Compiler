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

struct FieldDef {
    std::string typeName;
    std::string fieldName;
};

inline llvm::Type* getTypeByName(const std::string& name) {
    if(name == "int") return Builder.getInt32Ty();
    if(name == "float") return Builder.getFloatTy();
    if(auto* sInfo = symbolTable.getStructInfo(name)) {
        return sInfo->type;
    }
    throw std::runtime_error("Unknown type: " + name);
}

extern std::vector<LoopBlocks> LoopStack;

inline llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function* TheFunction, const std::string& VarName, llvm::Type* VarTy) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(VarTy, nullptr, VarName);
}

//Base class for all nodes
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0; // For printing AST
    virtual llvm::Value* codegen() = 0; // Generating IR.
};

class ExprNode : public ASTNode {};

class VariableExprAST : public ExprNode { 
public:
    std::string name;

    explicit VariableExprAST(std::string name) : name(std::move(name)) {}

    void print(int indent = 0) const override {
        std::string space(indent * 2, ' ');
        std::cout << space << "Variable(" << name << ")\n";
    }

    llvm::Value* codegen() override {
        llvm::AllocaInst* A = symbolTable.lookupVariable(name).Alloca;
        if(!A) {
            std::cerr << "Unknown Variable name: " << name << std::endl;
            return nullptr;
        }
        return Builder.CreateLoad(A->getAllocatedType(), A, name.c_str());
    }
};

class StructDeclAST : public ASTNode {
public:
    std::string structName;
    std::vector<FieldDef> fields;

    StructDeclAST(std::string Name, std::vector<FieldDef> Fields) : structName(std::move(Name)), fields(std::move(Fields)) {}

    llvm::Value* codegen() override {
        std::vector<llvm::Type*> bodyTypes;
        StructTypeInfo info;

        unsigned index = 0;
        for(const auto &field : fields) {
            llvm::Type* fType = getTypeByName(field.typeName);
            bodyTypes.push_back(fType);

            info.fields[field.fieldName] = {index++, fType};
        }

        info.type = llvm::StructType::create(Context, bodyTypes, "struct." + structName);

        symbolTable.registerStruct(structName, info);

        return nullptr; // Type Declaration do NOT generates runtime-code
    }

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
    
    llvm::Value* codegen() override {
        std::string varName;
        if(VariableExprAST* varExpr = dynamic_cast<VariableExprAST*>(base.get())) {
            varName = varExpr->name;
        } else {
            throw std::runtime_error("Nested struct access codegen is under construction");
        }

        auto symbInfo = symbolTable.lookupVariable(varName);
        auto structInfo = symbolTable.getStructInfo(symbInfo.typeName);
        if(!symbInfo.Alloca || !structInfo) {
            throw std::runtime_error("Unknown struct variable: " + varName);
        }

        auto fieldIt = structInfo->fields.find(fieldName);
        if(fieldIt == structInfo->fields.end()) {
            throw std::runtime_error("Struct " + symbInfo.typeName + " has no field named " + fieldName);
        }

        unsigned fieldIdx = fieldIt->second.first;
        llvm::Type* fieldType = fieldIt->second.second;
        
        llvm::Value* fieldPtr = Builder.CreateGEP(structInfo->type, symbInfo.Alloca, {Builder.getInt32(0), Builder.getInt32(fieldIdx)}, varName + "." + fieldName + "ptr");

        return Builder.CreateLoad(fieldType, fieldPtr, fieldName + "val");
    }

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

    llvm::Value* codegen() override {
        auto symbInfo = symbolTable.lookupVariable(varName);
        auto structInfo = symbolTable.getStructInfo(symbInfo.typeName);

        unsigned fieldIdx = structInfo->fields.at(fieldName).first;
        llvm::Value* valToStore = value->codegen();

        llvm::Value* idxList[] = {Builder.getInt32(0), Builder.getInt32(fieldIdx)};

        llvm::Value* fieldPtr = Builder.CreateGEP(structInfo->type, symbInfo.Alloca, idxList, varName + "." + fieldName + ".ptr");

        return Builder.CreateStore(valToStore, fieldPtr);
    }

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

    llvm::Value* codegen() override {
        std::string name;
        if(auto* varExpr = dynamic_cast<VariableExprAST*>(base.get())) {
            name = varExpr->name;
        } else {

        }

        llvm::AllocaInst* Alloca = symbolTable.lookupVariable(name).Alloca;
        if(!Alloca) {
            std::cerr << "Unknown variable name: " << name << std::endl;
            return nullptr;
        }

        llvm::Value* IndexVal = index->codegen();
        if(!IndexVal) return nullptr;

        llvm::Value* IdxList[] = {Builder.getInt32(0), IndexVal};

        llvm::Type* ArrayTy = Alloca->getAllocatedType();

        llvm::Value* ElementPtr = Builder.CreateGEP(ArrayTy, Alloca, IdxList, "arrayidx");

        return Builder.CreateLoad(Builder.getInt32Ty(), ElementPtr, "tmpld");
    }

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

    llvm::Value* codegen() override {
        llvm::ArrayType* ArrayTy = llvm::ArrayType::get(Builder.getInt32Ty(), size);

        llvm::AllocaInst* Alloca = Builder.CreateAlloca(ArrayTy, nullptr, name);

        uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(ArrayTy);
        llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);

        Builder.CreateLifetimeStart(Alloca, sizeVal);

        symbolTable.declareVariable(name, Alloca, "array");

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
        llvm::AllocaInst* Alloca = symbolTable.lookupVariable(name).Alloca;
        if(!Alloca) return nullptr;

        llvm::Value* idxVal = index->codegen();
        llvm::Value* valToStore = value->codegen();

        llvm::Value* idxList[] = {Builder.getInt32(0), idxVal};
        llvm::Type* arrayTy = Alloca->getAllocatedType();

        llvm::Value* elemPtr = Builder.CreateGEP(arrayTy, Alloca, idxList, "arrayidx");

        return Builder.CreateStore(valToStore, elemPtr);
    }

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "WriteintoArray(name: " << name << ", index: " << index << ", val: " << value << ")\n"; 
    }
};

/*---------------------------------------------------------------------------------------------------------------------*/

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
        if(CondV->getType()->isIntegerTy() && CondV->getType()->getIntegerBitWidth() != 1) {
            CondV = Builder.CreateICmpNE(CondV, Builder.getInt32(0), "loopcond");
        }

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

/*---------------------------------------------------------------------------------------------------------*/

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

        for(auto const &[name, symbolInfo] : LocalVariables) {
            if(symbolInfo.Alloca) {
            // Gain type size in bytes

                if(Builder.GetInsertBlock() && Builder.GetInsertBlock()->getTerminator()) {
                    break;
                }
                llvm::Type* varType = symbolInfo.Alloca->getAllocatedType();
                uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(varType);
                llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);


                Builder.CreateLifetimeEnd(symbolInfo.Alloca, sizeVal);
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

/*----------------------------------------------------------*/

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
            std::string(Arg.getName()), Alloca, "int");
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
    llvm::Value* codegen() override {
        llvm::Value* CondV = Condition->codegen();

        if(CondV->getType()->isIntegerTy() && CondV->getType()->getIntegerBitWidth() != 1) {
            CondV = Builder.CreateICmpNE(CondV, Builder.getInt32(0), "ifcond");
        }

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

class FloatExprAST : public ExprNode {
    float val;
public:
    FloatExprAST(float Val) : val(Val) {}
    llvm::Value* codegen() override {
        return llvm::ConstantFP::get(Context, llvm::APFloat(val));
    }

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
    
    llvm::Value* codegen() override {
        if(op == "=") {
            llvm::Value* value = right->codegen();
            if(!value) return nullptr;

            if(auto* variable = dynamic_cast<VariableExprAST*>(left.get())) {
                auto symbolInfo = symbolTable.lookupVariable(variable->name);
                if(!symbolInfo.Alloca) {
                    throw std::runtime_error("Unknown variable: " + variable->name);
                }
                Builder.CreateStore(value, symbolInfo.Alloca);
                return value;
            }

            if(auto* access = dynamic_cast<StructAccessAST*>(left.get())) {
                auto* base = dynamic_cast<VariableExprAST*>(access->base.get());
                if(!base) {
                    throw std::runtime_error("Nested struct assignment is under construction");
                }

                auto symbolInfo = symbolTable.lookupVariable(base->name);
                auto* structInfo = symbolTable.getStructInfo(symbolInfo.typeName);
                if(!symbolInfo.Alloca || !structInfo) {
                    throw std::runtime_error("Unknown struct variable: " + base->name);
                }

                auto fieldIt = structInfo->fields.find(access->fieldName);
                if(fieldIt == structInfo->fields.end()) {
                    throw std::runtime_error("Unknown struct field: " + access->fieldName);
                }

                auto fieldPtr = Builder.CreateGEP(
                    structInfo->type,
                    symbolInfo.Alloca,
                    {Builder.getInt32(0), Builder.getInt32(fieldIt->second.first)},
                    base->name + "." + access->fieldName + ".ptr");
                Builder.CreateStore(value, fieldPtr);
                return value;
            }

            throw std::runtime_error("Invalid assignment target");
        }

        llvm::Value* L = left->codegen();
        llvm::Value* R = right->codegen();
        if(!L || !R) {
            return nullptr;
        }

        bool isFloat = L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy();

        if(op == "<") {
            return isFloat ? Builder.CreateFCmpOLT(L, R, "cmptmp") : Builder.CreateICmpSLT(L, R, "cmptmp");
        }
        if(op == ">") {
            return isFloat ? Builder.CreateFCmpOGT(L, R, "cmptmp") : Builder.CreateICmpSGT(L, R, "cmptmp");
        }
        if(op == "==") {
            return isFloat ? Builder.CreateFCmpOEQ(L, R, "cmptmp") : Builder.CreateICmpEQ(L, R, "cmptmp");
        }
        if(op == "!=") {
            return isFloat ? Builder.CreateFCmpONE(L, R, "cmptmp") : Builder.CreateICmpNE(L, R, "cmptmp");
        }
        if(op == "<=") {
            return isFloat ? Builder.CreateFCmpOLE(L, R, "cmptmp") : Builder.CreateICmpSLE(L, R, "cmptmp");
        }
        if(op == ">=") {
            return isFloat ? Builder.CreateFCmpOGE(L, R, "cmptmp") : Builder.CreateICmpSGE(L, R, "cmptmp");
        }
        if(op == "+") return isFloat ? Builder.CreateFAdd(L, R, "addtmp") : Builder.CreateAdd(L, R, "addtmp");
        if(op == "-") return isFloat ? Builder.CreateFSub(L, R, "subtmp") : Builder.CreateSub(L, R, "subtmp");
        if(op == "*") return isFloat ? Builder.CreateFMul(L, R, "multmp") : Builder.CreateMul(L, R, "multmp");
        if(op == "/") return isFloat ? Builder.CreateFDiv(L, R, "divtmp") : Builder.CreateSDiv(L, R, "divtmp");

        throw std::runtime_error("Unknown binary operator: " + op);
    }

};

class UnaryMinusExprAST : public ExprNode {
public:
    char op;
    std::unique_ptr<ExprNode> operand;
    UnaryMinusExprAST(char Op, std::unique_ptr<ExprNode> Operand) : op(Op), operand(std::move(Operand)) {}

    llvm::Value* codegen() override {
        llvm::Value* operandVal = operand->codegen();
        if(!operandVal) return nullptr;

        switch (op) {
            case '-': return Builder.CreateNeg(operandVal, "negtmp");
            default:
                throw std::runtime_error("Unknown unary operator: " + op);
        }
    };

    void print(int indent = 0) const override {
        std::string space(indent*2, ' ');
        std::cout << space << "Unary Expression(op: " << op << ", operand: " << operand;
    }
};

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

    llvm::Value* codegen() override {
        llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();
        llvm::Type* VarTy = getTypeByName(typeName);
        
        if(!VarTy) {
            std::cerr << "Error: Unknown type " << typeName << std::endl;
            return nullptr;
        }

        llvm::AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, name, VarTy);

        if(initializer) {
            llvm::Value* initVal = initializer->codegen();
            if(!initVal) return nullptr;
            Builder.CreateStore(initVal, Alloca);
        }

        uint64_t typeSize = TheModule->getDataLayout().getTypeAllocSize(VarTy);
        llvm::ConstantInt* sizeVal = Builder.getInt64(typeSize);
        Builder.CreateLifetimeStart(Alloca, sizeVal);

        symbolTable.declareVariable(name, Alloca, typeName);

        return Alloca;
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