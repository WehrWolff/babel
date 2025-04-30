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
#include <map>
#include <iostream>
#include <variant>
#include <vector>

#include "util.hpp"

// Base class for all expression node
class BaseAST {
    public:
        virtual ~BaseAST() = default;
        virtual llvm::Value *codegen() = 0;
};

// class for referencing variables
class VariableAST : public BaseAST {
    const std::string Name;

    public:
        explicit VariableAST (const std::string &Name) : Name(Name) {}
        llvm::Value *codegen() override;
};

class VariableDeclAST : public BaseAST {
    std::vector<std::pair<std::string, std::unique_ptr<BaseAST>>> VarNames;
    std::unique_ptr<BaseAST> Body;

    public:
        VariableDeclAST(std::vector<std::pair<std::string, std::unique_ptr<BaseAST>>> VarNames, std::unique_ptr<BaseAST> Body) : VarNames(std::move(VarNames)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
};

// class for numeric literals which are integers
class IntegerAST : public BaseAST {
    const int Val;

    public:
        explicit IntegerAST (int Val) : Val(Val) {}
        llvm::Value *codegen() override;
};

// class for numeric literals which are floating points
class FloatingPointAST : public BaseAST {
    const double Val;

    public:
        explicit FloatingPointAST (double Val) : Val(Val) {}
        llvm::Value *codegen() override;
};

// class for when binary operators are used
class BinaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> LHS;
    std::unique_ptr<BaseAST> RHS;

    public:
        BinaryOperatorAST (const std::string& Op, std::unique_ptr<BaseAST> LHS, std::unique_ptr<BaseAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
};

// class for when a function is called
class TaskCallAST : public BaseAST {
    const std::string callsTo;
    std::vector<std::unique_ptr<BaseAST>> Args;

    public:
        TaskCallAST (const std::string &callsTo, std::vector<std::unique_ptr<BaseAST>> Args) : callsTo(callsTo), Args(std::move(Args)) {}
        llvm::Value *codegen() override;
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    const std::string Name;
    std::vector<std::string> Args;

    public:
        TaskHeaderAST (const std::string &Name, std::vector<std::string> Args) : Name(Name), Args(std::move(Args)) {}
        llvm::Function *codegen() override;
        const std::string &getName() const { return Name; }
};

// class for the function definition
class TaskAST : public BaseAST {
    std::unique_ptr<TaskHeaderAST> Header;
    std::unique_ptr<BaseAST> Body;

    public:
        TaskAST (std::unique_ptr<TaskHeaderAST> Header, std::unique_ptr<BaseAST> Body) : Header(std::move(Header)), Body(std::move(Body)) {}
        llvm::Function *codegen() override;
};

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::unique_ptr<llvm::Module> TheModule;
static std::map<std::string, llvm::AllocaInst *> NamedValues;

llvm::Value *LogError(const char *str) {
    std::cerr << str << '\n';
    return nullptr;
}

llvm::Value *FloatingPointAST::codegen() {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *IntegerAST::codegen() {
    // bit width 32
    return llvm::ConstantInt::get(*TheContext, llvm::APInt(32, Val, true));
}

llvm::Value *VariableAST::codegen() {
    llvm::Value *V = NamedValues[Name];
    if (!V) LogError("error");
    return V;
}

llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::StringRef VarName) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
    TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr, VarName);
}

llvm::Value *VariableDeclAST::codegen() {
    std::vector<llvm::AllocaInst *> OldBindings;

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
        const std::string &VarName = VarNames[i].first;
        BaseAST *Init = VarNames[i].second.get();

        // Emit the initializer before adding the variable to scope, this prevents
        // the initializer from referencing the variable itself, and permits stuff
        // like this:
        //  var a = 1 in
        //    var a = a in ...   # refers to outer 'a'.
        llvm::Value *InitVal;
        if (Init) {
        InitVal = Init->codegen();
        if (!InitVal)
            return nullptr;
        } else { // If not specified, use 0.0.
        InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
        }

        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder->CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding.
        NamedValues[VarName] = Alloca;
    }

    // Codegen the body, now that all vars are in scope.
    llvm::Value *BodyVal = Body->codegen();
    if (!BodyVal)
        return nullptr;

    // Pop all our variables from scope.
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
        NamedValues[VarNames[i].first] = OldBindings[i];

    // Return the body computation.
    return BodyVal;
}

llvm::Value *BinaryOperatorAST::codegen() {
    llvm::Value *left = LHS->codegen();
    llvm::Value *right = RHS->codegen();
    if (!left || !right) return nullptr;

    if (Op == "+") {
        return Builder->CreateAdd(left, right, "addtmp");
    } else if (Op == "-") {
        return Builder->CreateSub(left, right, "subtmp");
    } else if (Op == "*") {
        return Builder->CreateMul(left, right, "multmp");
    } else if (Op == "/") {
        llvm::Type* doubleTy = llvm::Type::getDoubleTy(*TheContext);

        llvm::Value* lhs_fp = Builder->CreateSIToFP(left, doubleTy, "lhsfp");
        llvm::Value* rhs_fp = Builder->CreateSIToFP(right, doubleTy, "rhsfp");
        return Builder->CreateFDiv(lhs_fp, rhs_fp, "divtmp");
    } else if (Op == "//") {
        return Builder->CreateSDiv(left, right, "idivtmp");
    } else if (Op == "%")
        return Builder->CreateSRem(left, right, "remtmp");
    else {
        return LogError("Invalid binary operator");
    }
}

llvm::Value *TaskCallAST::codegen() {
    llvm::Function *CalleF = TheModule->getFunction(callsTo);
    if (!CalleF) return LogError("Unknown Task referenced");

    if (CalleF->arg_size() != Args.size()) return LogError("Passed incorrect number of arguments");

    std::vector<llvm::Value *> ArgsV;
    for (unsigned int i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back()) return nullptr;
    }

    return Builder->CreateCall(CalleF, ArgsV, "calltmp");
}

llvm::Function *TaskHeaderAST::codegen() {
    // ! currently double, change later
    std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext));
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

    unsigned int idx = 0;
    for (auto &Arg : F->args()) {
        Arg.setName(Args[idx++]);
    }

    return F;
}

llvm::Function *TaskAST::codegen() {
    llvm::Function *TheFunction = TheModule->getFunction(Header->getName());
    if (!TheFunction) TheFunction = Header->codegen();
    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) return (llvm::Function*)LogError("Task cannot be redefined");

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());
        Builder->CreateStore(&Arg, Alloca);
        NamedValues[std::string(Arg.getName())] = Alloca;
    }

    if (llvm::Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);
        verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return nullptr;
}

class RootAST {
public:
    std::vector<std::unique_ptr<BaseAST>> statement_list;

    RootAST() = default;

    void addASTNode(std::unique_ptr<BaseAST> node) {
        //statement_list.push_back(node);
        babel_stub();
    }
};