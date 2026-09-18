#include "ast.h"
#include "codegenHelpers.h"

llvm::Value* VariableExprAST::codegen() {
    auto symbolInfo = symbolTable.lookupVariable(name);
    if(!symbolInfo.Alloca) {
        std::cerr << "Unknown Variable name: " << name << std::endl;
        return nullptr;
    }
    return Builder.CreateLoad(symbolInfo.Alloca->getAllocatedType(), codegenAddress(), name.c_str());
}

llvm::Value* StructDeclAST::codegen() {
    std::vector<llvm::Type*> bodyTypes;
    StructTypeInfo info;
    unsigned index = 0;

    for(const auto& field : fields) {
        llvm::Type* fieldType = getTypeByName(field.typeName);

        bodyTypes.push_back(fieldType);
        info.fields[field.fieldName] = {index++, fieldType};
    }

    info.type = llvm::StructType::create(Context, bodyTypes, "struct." + structName);

    symbolTable.registerStruct(structName, info);

    return nullptr;
}

llvm::Value* StructAccessAST::codegen() {
    llvm::Value* fieldPointer = codegenAddress();
    if(!fieldPointer) return nullptr;

    auto* variable = dynamic_cast<VariableExprAST*>(base.get());
    auto symbolInfo = symbolTable.lookupVariable(variable->name);
    auto* structInfo = symbolTable.getStructInfo(symbolInfo.typeName);
    auto field = structInfo->fields.find(fieldName);
    return Builder.CreateLoad(field->second.second, fieldPointer, fieldName + ".val");
}

llvm::Value* StructAccessAST::codegenAddress() {
    auto* variable = dynamic_cast<VariableExprAST*>(base.get());
    if(!variable) throw std::runtime_error("Nested struct access codegen is under construction");

    auto symbolInfo = symbolTable.lookupVariable(variable->name);

    auto* structInfo = symbolTable.getStructInfo(symbolInfo.typeName);
    if(!symbolInfo.Alloca || !structInfo) throw std::runtime_error("Unknown struct variable: " + variable->name);

    auto field = structInfo->fields.find(fieldName);
    if(field == structInfo->fields.end()) throw std::runtime_error("Unknown struct field: " + fieldName);

    llvm::Value* fieldPtr = Builder.CreateGEP(
        structInfo->type,
        symbolInfo.Alloca,
        {Builder.getInt32(0), Builder.getInt32(field->second.first)},
        variable->name + "." + fieldName + ".ptr");

    return fieldPtr;
}

llvm::Value* StructAssignAST::codegen() {
    auto symbolInfo = symbolTable.lookupVariable(varName);

    auto* structInfo = symbolTable.getStructInfo(symbolInfo.typeName);
    if(!symbolInfo.Alloca || !structInfo) return nullptr;

    auto field = structInfo->fields.find(fieldName);
    if(field == structInfo->fields.end()) return nullptr;

    llvm::Value* valueToStore = value->codegen();
    if(!valueToStore) return nullptr;

    llvm::Value* fieldPtr = Builder.CreateGEP(
        structInfo->type,
        symbolInfo.Alloca,
        {Builder.getInt32(0), Builder.getInt32(field->second.first)},
        varName + "." + fieldName + ".ptr");
        
    return Builder.CreateStore(valueToStore, fieldPtr);
}

llvm::Value* ArrayAccessAST::codegen() {
    llvm::Value* elementPointer = codegenAddress();
    if(!elementPointer) return nullptr;
    return Builder.CreateLoad(Builder.getInt32Ty(), elementPointer, "array.value");
}

llvm::Value* ArrayAccessAST::codegenAddress() {
    auto* variable = dynamic_cast<VariableExprAST*>(base.get());
    if(!variable) throw std::runtime_error("Nested array access is under construction");

    auto arrayInfo = symbolTable.lookupVariable(variable->name);
    if(!arrayInfo.Alloca) return nullptr;

    llvm::Value* indexValue = index->codegen();
    if(!indexValue) return nullptr;

    llvm::Value* elementPtr = createArrayElementPointer(arrayInfo, indexValue, "arrayidx");

    return elementPtr;
}

llvm::Value* ArrayDeclAST::codegen() {
    llvm::ArrayType* arrayType = llvm::ArrayType::get(Builder.getInt32Ty(), size);

    llvm::AllocaInst* alloca = Builder.CreateAlloca(arrayType, nullptr, name);

    llvm::ConstantInt* lifetimeSize = Builder.getInt64(TheModule->getDataLayout().getTypeAllocSize(arrayType));

    Builder.CreateLifetimeStart(alloca, lifetimeSize);

    symbolTable.declareVariable(name, alloca, "int", VariableType::StaticArray, lifetimeSize);

    return alloca;
}

llvm::Value* ArrayAssignAST::codegen() {
    auto arrayInfo = symbolTable.lookupVariable(name);
    if(!arrayInfo.Alloca) return nullptr;

    llvm::Value* indexValue = index->codegen();

    llvm::Value* valueToStore = value->codegen();
    if(!indexValue || !valueToStore) return nullptr;

    llvm::Value* elementPtr = createArrayElementPointer(arrayInfo, indexValue, "arrayidx");

    return Builder.CreateStore(valueToStore, elementPtr);
}

llvm::Value* VLADeclAST::codegen() {
    llvm::Value* count = sizeExpr->codegen();
    if(!count) return nullptr;

    llvm::AllocaInst* alloca = Builder.CreateAlloca(Builder.getInt32Ty(), count, arrayName);

    llvm::ConstantInt* unknownSize = Builder.getInt64(-1);

    Builder.CreateLifetimeStart(alloca, unknownSize);

    symbolTable.declareVariable(arrayName, alloca, "int", VariableType::RuntimeArray, unknownSize);
    return alloca;
}

llvm::Value* BreakAST::codegen() {
    if(LoopStack.empty()) return nullptr;

    Builder.CreateBr(LoopStack.back().AfterBB);

    llvm::Function* function = Builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* dead = llvm::BasicBlock::Create(Context, "dead", function);

    Builder.SetInsertPoint(dead);

    return llvm::Constant::getNullValue(Builder.getInt32Ty());
}

llvm::Value* ContinueAST::codegen() {
    if(LoopStack.empty()) return nullptr;

    Builder.CreateBr(LoopStack.back().CondBB);

    llvm::Function* function = Builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* dead = llvm::BasicBlock::Create(Context, "dead", function);

    Builder.SetInsertPoint(dead);

    return llvm::Constant::getNullValue(Builder.getInt32Ty());
}

llvm::Value* WhileLoopAST::codegen() {
    llvm::Function* function = Builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* conditionBlock = llvm::BasicBlock::Create(Context, "whilecond", function);
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(Context, "whilebody", function);
    llvm::BasicBlock* afterBlock = llvm::BasicBlock::Create(Context, "whileafter", function);

    Builder.CreateBr(conditionBlock);
    Builder.SetInsertPoint(conditionBlock);

    llvm::Value* condition = cond->codegen();
    if(!condition) return nullptr;

    if(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() != 1) {
        condition = Builder.CreateICmpNE(condition, Builder.getInt32(0), "loopcond");
    }
    Builder.CreateCondBr(condition, bodyBlock, afterBlock);

    Builder.SetInsertPoint(bodyBlock);

    LoopStack.push_back({conditionBlock, afterBlock});
    body->codegen();

    LoopStack.pop_back();

    if(!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(conditionBlock);

    Builder.SetInsertPoint(afterBlock);

    return llvm::Constant::getNullValue(Builder.getInt32Ty());
}

llvm::Value* BlockAST::codegen() {
    symbolTable.pushScope();

    for(auto& statement : Statements) {
        if(statement) statement->codegen();

        if(Builder.GetInsertBlock() && Builder.GetInsertBlock()->getTerminator()) break;
    }
    auto localVariables = symbolTable.popScope();

    for(const auto& [name, symbolInfo] : localVariables) {
        if(Builder.GetInsertBlock() && Builder.GetInsertBlock()->getTerminator()) break;

        endLifetime(symbolInfo);
    }
    return nullptr;
}

llvm::Value* CallExprAST::codegen() {
    llvm::Function* callee = TheModule->getFunction(Callee);
    if(!callee) return nullptr;

    std::vector<llvm::Value*> arguments;

    for(auto& argument : Args) arguments.push_back(argument->codegen());

    return Builder.CreateCall(callee, arguments, "calltmp");
}

llvm::Value* PrototypeAST::codegen() {
    std::vector<llvm::Type*> types(Args.size(), llvm::Type::getInt32Ty(Context));

    llvm::FunctionType* functionType = llvm::FunctionType::get(llvm::Type::getInt32Ty(Context), types, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, Name, TheModule.get());

    unsigned index = 0;

    for(auto& argument : function->args()) argument.setName(Args[index++]);

    return function;
}

llvm::Value* FunctionAST::codegen() {
    llvm::Function* function = TheModule->getFunction(Prototype->Name);

    if(!function) {
        Prototype->codegen();
        function = TheModule->getFunction(Prototype->Name);
    }
    if(!function || !function->empty()) return nullptr;

    Builder.SetInsertPoint(llvm::BasicBlock::Create(Context, "entry", function));

    symbolTable.pushScope();

    for(auto& argument : function->args()) {
        llvm::IRBuilder<> entryBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
        llvm::AllocaInst* alloca = entryBuilder.CreateAlloca(Builder.getInt32Ty(), nullptr, std::string(argument.getName()));
        Builder.CreateStore(&argument, alloca);
        symbolTable.declareVariable(std::string(argument.getName()), alloca, "int");
    }
    Body->codegen();

    symbolTable.popScope();
    return function;
}

llvm::Value* ReturnStmtAST::codegen() {
    llvm::Value* returnValue = Value ? Value->codegen() : Builder.getInt32(0);

    endLifetimes(symbolTable.activeVariables());

    return Builder.CreateRet(returnValue);
}

llvm::Value* IfStmtAST::codegen() {
    llvm::Value* condition = Condition->codegen();
    if(!condition) return nullptr;

    if(condition->getType()->isIntegerTy() && condition->getType()->getIntegerBitWidth() != 1) {
        condition = Builder.CreateICmpNE(condition, Builder.getInt32(0), "ifcond");
    }

    llvm::Function* function = Builder.GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(Context, "then", function);
    llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(Context, "else");
    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(Context, "ifcont");

    Builder.CreateCondBr(condition, thenBlock, elseBlock);

    Builder.SetInsertPoint(thenBlock);
    Then->codegen();
    if(!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(mergeBlock);

    function->insert(function->end(), elseBlock);
    Builder.SetInsertPoint(elseBlock);
    if(Else) Else->codegen();

    if(!Builder.GetInsertBlock()->getTerminator()) Builder.CreateBr(mergeBlock);

    function->insert(function->end(), mergeBlock);
    Builder.SetInsertPoint(mergeBlock);
    return nullptr;
}

llvm::Value* NumberExprAST::codegen() {
    return llvm::ConstantInt::get(Context, llvm::APInt(32, value, true));
}

llvm::Value* FloatExprAST::codegen() {
    return llvm::ConstantFP::get(Context, llvm::APFloat(val));
}

llvm::Value* BinaryExprAST::codegen() {
    if(op == "=") {
        llvm::Value* address = left->codegenAddress();
        llvm::Value* value = right->codegen();
        if(!address || !value) throw std::runtime_error("Invalid assignment");
        Builder.CreateStore(value, address);
        return value;
    }

    llvm::Value* leftValue = left->codegen();
    llvm::Value* rightValue = right->codegen();
    if(!leftValue || !rightValue) return nullptr;

    bool isFloat = leftValue->getType()->isFloatingPointTy() || rightValue->getType()->isFloatingPointTy();
    if(op == "<") return isFloat ? Builder.CreateFCmpOLT(leftValue, rightValue) : Builder.CreateICmpSLT(leftValue, rightValue);
    if(op == ">") return isFloat ? Builder.CreateFCmpOGT(leftValue, rightValue) : Builder.CreateICmpSGT(leftValue, rightValue);
    if(op == "==") return isFloat ? Builder.CreateFCmpOEQ(leftValue, rightValue) : Builder.CreateICmpEQ(leftValue, rightValue);
    if(op == "!=") return isFloat ? Builder.CreateFCmpONE(leftValue, rightValue) : Builder.CreateICmpNE(leftValue, rightValue);
    if(op == "<=") return isFloat ? Builder.CreateFCmpOLE(leftValue, rightValue) : Builder.CreateICmpSLE(leftValue, rightValue);
    if(op == ">=") return isFloat ? Builder.CreateFCmpOGE(leftValue, rightValue) : Builder.CreateICmpSGE(leftValue, rightValue);
    if(op == "+") return isFloat ? Builder.CreateFAdd(leftValue, rightValue) : Builder.CreateAdd(leftValue, rightValue);
    if(op == "-") return isFloat ? Builder.CreateFSub(leftValue, rightValue) : Builder.CreateSub(leftValue, rightValue);
    if(op == "*") return isFloat ? Builder.CreateFMul(leftValue, rightValue) : Builder.CreateMul(leftValue, rightValue);
    if(op == "/") return isFloat ? Builder.CreateFDiv(leftValue, rightValue) : Builder.CreateSDiv(leftValue, rightValue);
    throw std::runtime_error("Unknown binary operator: " + op);
}

llvm::Value* UnaryMinusExprAST::codegen() {
    llvm::Value* value = operand->codegen();

    if(!value) return nullptr;
    if(op == '-') return Builder.CreateNeg(value, "negtmp");

    throw std::runtime_error("Unknown unary operator");
}

llvm::Value* VarDecAST::codegen() {
    llvm::Function* function = Builder.GetInsertBlock()->getParent();

    llvm::Type* variableType = getTypeByName(typeName);
    
    llvm::AllocaInst* alloca = createEntryBlockAlloca(function, name, variableType);
    llvm::ConstantInt* lifetimeSize = Builder.getInt64(TheModule->getDataLayout().getTypeAllocSize(variableType));
    Builder.CreateLifetimeStart(alloca, lifetimeSize);

    if(initializer) {
        llvm::Value* initialValue = initializer->codegen();
        if(!initialValue) return nullptr;
        Builder.CreateStore(initialValue, alloca);
    }
    symbolTable.declareVariable(name, alloca, typeName, VariableType::Basic, lifetimeSize);

    return alloca;
}

llvm::Value* ProgramAST::codegen() {
    llvm::Value* lastValue = nullptr;
    
    for(auto& statement : statements) if(statement) lastValue = statement->codegen();
    return lastValue;
}
