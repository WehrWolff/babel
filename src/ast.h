#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <memory>
#include <string>
#include <vector>

// Base class for all expression node
class BaseAST {
    public:
        virtual ~BaseAST() = default;
        virtual Value *codegen() = 0;
};

// class for referencing variables
class VariableAST : public BaseAST {
    const std::string Name;

    public:
        explicit VariableAST (const std::string &Name) : Name(Name) {}
        Value *codegen() override;
};

// class for numeric literals which are integers
class IntegerAST : public BaseAST {
    const int Val;

    public:
        explicit IntegerAST (int Val) : Val(Val) {}
        Value *codegen() override;
};

// class for numeric literals which are floating points
class FloatingPointAST : public BaseAST {
    const double Val;

    public:
        explicit FloatingPointAST (double Val) : Val(Val) {}
        Value *codegen() override;
};

// class for when binary operators are used
class BinaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> LHS, RHS;

    public:
        BinaryOperatorAST (std::string Op, std::unique_ptr<BaseAST> LHS, std::unique_ptr<BaseAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        Value *codegen() override;
};

// class for when a function is called
class TaskCallAST : public BaseAST {
    const std::string callsTo;
    std::vector<std::unique_ptr<BaseAST>> Args;

    public:
        TaskCallAST (const std::string &callsTo, std::vector<std::unique_ptr<BaseAST>> Args) : callsTo(callsTo), Args(std::move(Args)) {}
        Value *codegen() override;
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    const std::string Name;
    std::vector<std::unique_ptr<BaseAST>> Args;

    public:
        TaskHeaderAST (const std::string &Name, std::vector<std::unique_ptr<BaseAST>> Args) : Name(Name), Args(std::move(Args)) {}
        Function *codegen() override;
};

// class for the function definition
class TaskAST : public BaseAST {
    std::unique_ptr<TaskHeaderAST> Header;
    std::unique_ptr<BaseAST> Body;

    public:
        TaskAST (std::unique_ptr<TaskHeaderAST> Header, std::unique_ptr<BaseAST> Body) : Header(std::move(Header)), Body(std::move(Body)) {}
        Function *codegen() override;
};

static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;

Value *LogError(const char *str) {
    std::cerr << str << '\n';
    return nullptr;
}

Value *FloatingPointAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Val))
}

Value *IntegerAST::codegen() {
    return ConstantInt::get(*TheContext, APInt(Val));
}

Value *VariableAST::codegen() {
    Value *V = NamedValues[Name];
    if (!V) LogError("error");
    return V;
}

Value *BinaryOperatorAST::codegen() {
    Value *left = LHS->codegen();
    Value *right = RHS->codegen();
    if (!left || !right) return nullptr;

    switch (Op) {
        case "+":
            return Builder->CreateAdd(left, right, "addtmp");
        case "-":
            return Builder->CreateSub(left, right, "subtmp");
        case "*":
            return Builder->CreateMul(left, right, "multmp");
        case "/":
            return Builder->CreateSDiv(left, right, "divtmp");
        default:
            return LogError("Invalid binary operator");
    }
}

Value *TaskCallAST::codegen() {
    Function *CalleF = TheModule->getFunction(callsTo);
    if (!CalleF) return LogError("Unknown Task referenced");

    if (CalleF->arg_size() != Args.size()) return LogError("Passed incorect number of arguments");

    std::vector<Value *> ArgsV;
    for (unsigned int i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back()) return nullptr;
    }

    return Builder->CreateCall(CalleF, ArgsV, "calltmp");
}

Function *TaskHeaderAST::codegen() {
    // ! currently double, change later
    std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    unsigned int idx = 0;
    for (auto &Arg : F->args()) {
        Arg.setName(Args[idx++]);
    }

    return F;
}

Function TaskAST::codegen() {
    Function *TheFunction = TheModule->getFunction(Proto->getName());
    if (!TheFunction) TheFunction = Proto->codegen();
    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) return (Function*)LogError("Task cannot be redefined");

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        NamedValues[std::string(arg.getName())] = &Arg;
    }

    if (Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);
        verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}