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

enum class BabelType {
    Int,
    Int8,
    Int16,
    Int32,
    Int64,
    Int128,
    // BigInt
    Float,
    Float16,
    Float32,
    Float64,
    Float128,
    Boolean,
    Character,
    CString,
    Void
};

// Base class for all expression node
class BaseAST {
    public:
        virtual ~BaseAST() = default;
        virtual llvm::Value *codegen() = 0;
        virtual BabelType getType() const { babel_panic("getType() not supported for this AST node"); }
};

// class for referencing variables
class VariableAST : public BaseAST {
    const std::string Name;
    const std::optional<BabelType> Type;
    const bool isConst;
    const bool isDecl;

    public:
        VariableAST(const std::string &Name, const std::optional<BabelType> Type, const bool isConst, const bool isDecl) : Name(Name), Type(Type), isConst(isConst), isDecl(isDecl) {
            if (isDecl) { insertSymbol(); }
        }
        void insertSymbol() const;
        std::string getName() const { return Name; }
        bool getConstness() const { return isConst; }
        bool getDecl() const { return isDecl; }
        BabelType getType() const override;
        llvm::Value *codegen() override;
};

class BooleanAST : public BaseAST {
    const int Val;

    public:
        explicit BooleanAST(std::string_view value) : Val(value == "TRUE" ? 1 : 0) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return BabelType::Boolean; }
};

// class for numeric literals which are integers
class IntegerAST : public BaseAST {
    const int Val;

    public:
        explicit IntegerAST(int Val) : Val(Val) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return BabelType::Int; }
};

class CharacterAST : public BaseAST {
    const char Val;

    public:
        explicit CharacterAST(const char Val) : Val(Val) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return BabelType::Character; }
};

class CStringAST : public BaseAST {
    const std::string Val;

    public:
        explicit CStringAST(const std::string& Val) : Val(Val) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return BabelType::CString; }
};

// class for numeric literals which are floating points
class FloatingPointAST : public BaseAST {
    const double Val;

    public:
        explicit FloatingPointAST(double Val) : Val(Val) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return BabelType::Float64; }
};

// class for when binary operators are used
class BinaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> LHS;
    std::unique_ptr<BaseAST> RHS;

    public:
        BinaryOperatorAST(const std::string& Op, std::unique_ptr<BaseAST> LHS, std::unique_ptr<BaseAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
        BabelType getType() const override;
};

class UnaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> Val;

    public:
        UnaryOperatorAST(const std::string& Op, std::unique_ptr<BaseAST> Val) : Op(Op), Val(std::move(Val)) {}
        llvm::Value *codegen() override;
};

class ReturnStmtAST : public BaseAST {
    std::unique_ptr<BaseAST> Expr;

    public:
        explicit ReturnStmtAST(std::unique_ptr<BaseAST> Expr) : Expr(std::move(Expr)) {}
        llvm::Value *codegen() override;
};

class GotoStmtAST : public BaseAST {
    std::string Target;

    public:
        explicit GotoStmtAST(const std::string& Target) : Target(Target) {}
        llvm::Value *codegen() override;
};

class LabelStmtAST : public BaseAST {
    std::string Name;

    public:
        explicit LabelStmtAST(const std::string& Name) : Name(Name) {}
        llvm::Value *codegen() override;
};

class BlockAST : public BaseAST {
    std::deque<std::unique_ptr<BaseAST>> Statements;

    public:
        explicit BlockAST(std::deque<std::unique_ptr<BaseAST>> Statements) : Statements(std::move(Statements)) {}
        llvm::Value *codegen() override;
};

class IfStmtAST : public BaseAST {
    std::unique_ptr<BaseAST> Cond;
    std::unique_ptr<BaseAST> Then;
    std::unique_ptr<BaseAST> Else;

    public:
        IfStmtAST(std::unique_ptr<BaseAST> Cond, std::unique_ptr<BaseAST> Then, std::unique_ptr<BaseAST> Else) : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
        llvm::Value *codegen() override;
};

// class for when a function is called
class TaskCallAST : public BaseAST {
    const std::string callsTo;
    std::deque<std::unique_ptr<BaseAST>> Args;

    public:
        TaskCallAST(const std::string &callsTo, std::deque<std::unique_ptr<BaseAST>> Args) : callsTo(callsTo), Args(std::move(Args)) {}
        BabelType getType() const override;
        llvm::Value *codegen() override;
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    const std::string Name;
    std::deque<std::string> Args;
    std::deque<BabelType> ArgTypes;
    BabelType ReturnType;

    public:
        TaskHeaderAST(const std::string &Name, std::deque<std::string> Args, std::deque<BabelType> ArgTypes, BabelType ReturnType) : Name(Name), Args(std::move(Args)), ArgTypes(std::move(ArgTypes)), ReturnType(ReturnType) {}
        llvm::Function *codegen() override;
        const std::string &getName() const { return Name; }
        const std::deque<BabelType> &getArgTypes() const { return ArgTypes; }
        const BabelType &getRetType() const { return ReturnType; }
};

// class for the function definition
class TaskAST : public BaseAST {
    std::unique_ptr<TaskHeaderAST> Header;
    std::unique_ptr<BaseAST> Body;

    public:
        TaskAST(std::unique_ptr<TaskHeaderAST> Header, std::unique_ptr<BaseAST> Body) : Header(std::move(Header)), Body(std::move(Body)) {}
        llvm::Function *codegen() override;
};

struct GlobalSymbol {
    llvm::GlobalVariable* val;
    BabelType type;
    bool isConstant;
};

struct LocalSymbol {
    llvm::AllocaInst* val;
    BabelType type;
    bool isConstant;
};

struct TaskTypeInfo {
    std::deque<BabelType> args;
    BabelType ret;
};

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::unique_ptr<llvm::Module> TheModule;
static std::map<std::string, LocalSymbol> NamedValues;
static std::map<std::string, GlobalSymbol> GlobalValues;
static std::map<std::string, llvm::BasicBlock*> LabelTable;
static std::map<std::string, TaskTypeInfo> TaskTable;

llvm::Value *LogError(const char *str) {
    std::cerr << str << '\n';
    return nullptr;
}

llvm::Type *resolveLLVMType(BabelType type) {
    using enum BabelType;
    switch (type) {
        case Int:
        case Int32:
            return llvm::Type::getInt32Ty(*TheContext);
        case Int8:
            return llvm::Type::getInt8Ty(*TheContext);
        case Int16:
            return llvm::Type::getInt16Ty(*TheContext);
        case Int64:
            return llvm::Type::getInt64Ty(*TheContext);
        case Int128:
            return llvm::Type::getInt128Ty(*TheContext);

        case Float:
        case Float32:
            return llvm::Type::getFloatTy(*TheContext);
        case Float16:
            return llvm::Type::getHalfTy(*TheContext);
        case Float64:
            return llvm::Type::getDoubleTy(*TheContext);
        case Float128:
            return llvm::Type::getFP128Ty(*TheContext);

        case Boolean:
            return llvm::Type::getInt1Ty(*TheContext);

        case CString:
            return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*TheContext));

        case Void:
            return llvm::Type::getVoidTy(*TheContext);

        default:
            babel_panic("Unknow type");
    }
}

BabelType getBabelType(const std::string& name) {
    // ignore optionals, arrays and template stuff for now


    using enum BabelType;
    const std::unordered_map<std::string, BabelType> TypeMap = {
        {"int", Int},
        {"int8", Int8},
        {"int16", Int16},
        {"int32", Int32},
        {"int64", Int64},
        {"int128", Int128},
        {"float", Float},
        {"float16", Float16},
        {"float32", Float32},
        {"float64", Float64},
        {"float128", Float128},
        {"bool", Boolean},
        {"char", Character},
        {"cstr", CString},
        {"void", Void},
    };

    return TypeMap.at(name);
}

std::string getBabelTypeName(BabelType type) {
    using enum BabelType;
    switch (type) {
        case Int:
        case Int32:     return "int32";
        case Int8:      return "int8";
        case Int16:     return "int16";
        case Int64:     return "int64";
        case Int128:    return "int128";

        case Float:
        case Float32:   return "float32";
        case Float16:   return "float16";
        case Float64:   return "float64";
        case Float128:  return "float128";

        case Boolean:   return "bool";
        case Character: return "char";
        case CString:   return "cstring";
        case Void:      return "void";

        default:
            babel_panic("Unknown type");
    }
}

bool isBabelInteger(BabelType type) {
    using enum BabelType;
    switch (type) {
        case Int8:
        case Int16:
        case Int32:
        case Int64:
        case Int128:
        case Int:
            return true;

        default:
            return false;
    }
}

bool isBabelFloat(BabelType type) {
    using enum BabelType;
    switch (type) {
        case Float16:
        case Float32:
        case Float64:
        case Float128:
        case Float:
            return true;

        default:
            return false;
    }
}

bool canImplicitCast(BabelType from, BabelType to) {
    if (from == to)
        return true;

    std::unordered_map<BabelType, std::vector<BabelType>> ImplicitCastTable = {
        {BabelType::Int8, {BabelType::Int16, BabelType::Int32, BabelType::Int64, BabelType::Int128, BabelType::Float16, BabelType::Float32, BabelType::Float64, BabelType::Float128}},
        {BabelType::Int16, {BabelType::Int32, BabelType::Int64, BabelType::Int128, BabelType::Float16, BabelType::Float32, BabelType::Float64, BabelType::Float128}},
        {BabelType::Int32, {BabelType::Int64, BabelType::Int128, BabelType::Float32, BabelType::Float64, BabelType::Float128}},
        {BabelType::Int64, {BabelType::Int128, BabelType::Float64, BabelType::Float128}},
        {BabelType::Int128, {BabelType::Float128}},
        // Character, CString, String
    };
    
    auto it = ImplicitCastTable.find(from);
    if (it == ImplicitCastTable.end()) return false;

    return std::ranges::find(it->second, to) != it->second.end();
}

llvm::Value *performImplicitCast(llvm::Value *val, BabelType from, BabelType to) {
    if (from == to)
        return val;

    if (isBabelInteger(from) && isBabelInteger(to)) {
        return Builder->CreateSExtOrBitCast(val, resolveLLVMType(to));
    } else if (isBabelInteger(from) && isBabelFloat(to)) {
        return Builder->CreateSIToFP(val, resolveLLVMType(to));
    } else if (isBabelFloat(from) && isBabelFloat(to)) {
        return Builder->CreateFPExt(val, resolveLLVMType(to));
    }

    babel_panic("Cannot perform illegal type cast");
}

void VariableAST::insertSymbol() const {
    // nullptr is not viewed as having a value
    // since we can't check using the Builder, just insert into both
    GlobalValues[Name] = {nullptr, Type.value(), isConst};
    NamedValues[Name] = {nullptr, Type.value(), isConst};
}

BabelType VariableAST::getType() const {
    if (Type.has_value())
        return Type.value();

    if (NamedValues.contains(Name))
        return NamedValues.at(Name).type;
    else if (GlobalValues.contains(Name))
        return GlobalValues.at(Name).type;
    else
        babel_panic("Unknown variable '%s' referenced", Name.c_str());
}

BabelType BinaryOperatorAST::getType() const {
    if (canImplicitCast(LHS->getType(), RHS->getType()))
        return RHS->getType();
    else if (canImplicitCast(RHS->getType(), LHS->getType()))
        return LHS->getType();
    else
        babel_panic("Cannot implicitly cast between %s and %s", getBabelTypeName(LHS->getType()).c_str(), getBabelTypeName(RHS->getType()).c_str());
}

BabelType TaskCallAST::getType() const  {
    return TaskTable.at(callsTo).ret;
}

llvm::Value *handleAssignment(llvm::Value* RHSVal, BabelType RHSType, BabelType VarType, const std::string& VarName, const bool isConst, const bool isDeclaration) {
    if (!RHSVal)
        return nullptr;

    if (Builder->GetInsertBlock()->getParent()->getName().starts_with("__global_main")) {
        // we are in global scope

        if (!llvm::isa<llvm::Constant>(RHSVal)) {
            //babel_panic("Global variables must be initialized with constant values");
        }

        if (GlobalValues.contains(VarName) && GlobalValues.at(VarName).val != nullptr) {
            if (isDeclaration) {
                babel_panic("Redefinition of global variable '%s'", VarName.c_str());
            }

            GlobalSymbol& existing = GlobalValues[VarName];
            if (existing.isConstant) {
                babel_panic("Cannot assign to constant '%s'", VarName.c_str());
            }

            if (canImplicitCast(RHSType, existing.type))
                RHSVal = performImplicitCast(RHSVal, RHSType, existing.type);

            Builder->CreateStore(RHSVal, existing.val);
            return existing.val;
        }

        if (!isDeclaration) {
            babel_panic("Variable '%s' used before declaration", VarName.c_str());
        }

        auto *GV = new llvm::GlobalVariable(
            *TheModule,
            resolveLLVMType(VarType),
            isConst,
            llvm::GlobalValue::ExternalLinkage,
            llvm::cast<llvm::Constant>(performImplicitCast(RHSVal, RHSType, VarType)), // must be a constant
            VarName
        );

        GlobalValues[VarName] = {GV, VarType, isConst};
        return GV;
    } else {
        // we are in local scope
        LocalSymbol Var = NamedValues[VarName];

        if (!Var.val) {
            if (GlobalValues.contains(VarName) && GlobalValues.at(VarName).val != nullptr) {
                if (isDeclaration) {
                    babel_panic("Redefinition of global variable '%s'", VarName.c_str());
                }
    
                GlobalSymbol& existing = GlobalValues[VarName];
                if (existing.isConstant) {
                    babel_panic("Cannot assign to constant '%s'", VarName.c_str());
                }

                if (canImplicitCast(RHSType, existing.type))
                    RHSVal = performImplicitCast(RHSVal, RHSType, existing.type);

                Builder->CreateStore(RHSVal, existing.val);
                return existing.val;
            }

            if (!isDeclaration) {
                babel_panic("Variable '%s' was never declared", VarName.c_str());
            }

            // Declare new variable
            llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
            llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
            Var.val = TmpB.CreateAlloca(resolveLLVMType(VarType), nullptr, VarName);
            Var.type = VarType;
            Var.isConstant = isConst;
            NamedValues[VarName] = {Var.val, VarType, isConst};
        } else {
            if (isDeclaration) {
                babel_panic("Redefinition of local variable '%s'", VarName.c_str());
            }

            if (Var.isConstant) {
                babel_panic("Cannot assign to constant '%s'", VarName.c_str());
            }
        }

        if (canImplicitCast(RHSType, Var.type))
            RHSVal = performImplicitCast(RHSVal, RHSType, Var.type);

        Builder->CreateStore(RHSVal, Var.val);
        return Var.val;
    }
}

llvm::Value *FloatingPointAST::codegen() {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *IntegerAST::codegen() {
    // bit width 32
    return llvm::ConstantInt::get(*TheContext, llvm::APInt(32, Val, true));
}

llvm::Value *CharacterAST::codegen() {
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*TheContext), Val);
}

llvm::Value *CStringAST::codegen() {
    llvm::Constant *ConstStr = llvm::ConstantDataArray::getString(*TheContext, Val, true);

    auto *GV = new llvm::GlobalVariable(
        *TheModule,
        ConstStr->getType(),
        true, // isConstant
        llvm::GlobalValue::PrivateLinkage,
        ConstStr,
        ".cstr");

    llvm::Constant *Zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
    llvm::Constant *Indices[] = {Zero, Zero};

    return llvm::ConstantExpr::getInBoundsGetElementPtr(ConstStr->getType(), GV, Indices);
}

llvm::Value *BooleanAST::codegen() {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*TheContext), Val);
}

llvm::Value *VariableAST::codegen() {
    if (NamedValues.contains(Name) && NamedValues.at(Name).val != nullptr) {
        if (NamedValues[Name].val->getType()->isPointerTy())
            return Builder->CreateLoad(resolveLLVMType(NamedValues[Name].type), NamedValues[Name].val, Name);
        
        return NamedValues[Name].val;
    } else if (GlobalValues.contains(Name) && GlobalValues.at(Name).val != nullptr) {
        if (GlobalValues[Name].val->getType()->isPointerTy())
            return Builder->CreateLoad(resolveLLVMType(GlobalValues[Name].type), GlobalValues[Name].val, Name);
        
        return GlobalValues[Name].val;
    } else {
        babel_panic("Unknown variable '%s' referenced", Name.c_str());
    }
}

llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::StringRef VarName) {
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(llvm::Type::getInt32Ty(*TheContext), nullptr, VarName); // i32 for now, change later
}


llvm::Value *BinaryOperatorAST::codegen() {
    if (Op == "=") {
        // LHS must be a variable
        const auto *Var = dynamic_cast<VariableAST*>(LHS.get());
        if (!Var)
            babel_panic("Destination of '=' must be a variable");

        llvm::Value *RHSVal = RHS->codegen();
        return handleAssignment(RHSVal, RHS->getType(), Var->getType(), Var->getName(), Var->getConstness(), Var->getDecl());
    }

    llvm::Value *left = LHS->codegen();
    llvm::Value *right = RHS->codegen();
    if (!left || !right) return nullptr;

    if (canImplicitCast(LHS->getType(), RHS->getType())) {
        left = performImplicitCast(left, LHS->getType(), RHS->getType());
    } else if (canImplicitCast(RHS->getType(), LHS->getType())) {
        right = performImplicitCast(right, RHS->getType(), LHS->getType());
    } else {
        babel_panic("Types dont match for binary operator; implicit cast failed or is not allowed");
    }

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
    } else if (Op == "%") {
        return Builder->CreateSRem(left, right, "remtmp");
    } else if (Op == "<<") {
        return Builder->CreateShl(left, right, "lshtmp");
    } else if (Op == ">>") {
        return Builder->CreateLShr(left, right, "rshtmp");
    } else if (Op == "|" || Op == "||") {
        return Builder->CreateOr(left, right, "ortmp");
    } else if (Op == "&" || Op == "&&") {
        return Builder->CreateAnd(left, right, "andtmp");
    } else if (Op == "^" || Op == "^^") {
        return Builder->CreateXor(left, right, "xortmp");
    } else if (Op == "==") {
        return Builder->CreateICmpEQ(left, right, "eqtmp");
    } else if (Op == "!=") {
        return Builder->CreateICmpNE(left, right, "netmp");
    } else if (Op == "<=") {
        return Builder->CreateICmpSLE(left, right, "letmp");
    } else if (Op == ">=") {
        return Builder->CreateICmpSGE(left, right, "getmp");
    } else if (Op == "<") {
        return Builder->CreateICmpSLT(left, right, "lttmp");
    } else if (Op == ">") {
        return Builder->CreateICmpSGT(left, right, "gttmp");
    } else {
        babel_panic("Invalid binary operator");
    }
}

llvm::Value *UnaryOperatorAST::codegen() {
    llvm::Value *operand = Val->codegen();
    if (!operand) return nullptr;

    if (Op == "!") {
        return Builder->CreateNot(operand, "nottmp");
    } else if (Op == "-") {
        return Builder->CreateNeg(operand, "negtmp");
    } else if (Op == "+") {
        return operand; // unary plus is a no-op
    } else {
        babel_panic("Invalid unary operator");
    }
}

llvm::Value *ReturnStmtAST::codegen() {
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    if (TheFunction->getName().starts_with("__global_main"))
        babel_panic("Return statements must be inside of a task");

    if (Expr) {
        llvm::Value *RetVal = Expr->codegen();
        if (canImplicitCast(Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret))
            RetVal = performImplicitCast(RetVal, Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret);
        Builder->CreateRet(RetVal);
    } else {
        Builder->CreateRetVoid();
    }

    // After return, future instructions in the block are dead, so create a new dummy block to prevent further emission
    //llvm::BasicBlock *AfterRetBB = llvm::BasicBlock::Create(*TheContext, "afterret", TheFunction);
    //Builder->SetInsertPoint(AfterRetBB);
    return nullptr;
}

llvm::Value *GotoStmtAST::codegen() {
    if (!LabelTable.contains(Target)) {
        //llvm::Function* TheFunction = Builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* TargetBB = llvm::BasicBlock::Create(*TheContext, Target);
        LabelTable[Target] = TargetBB;
    }

    Builder->CreateBr(LabelTable[Target]);

    // After a branch, we need to insert a new block for further instructions (if any)
    // llvm::BasicBlock* DeadBB = llvm::BasicBlock::Create(*TheContext, "after_goto", Builder->GetInsertBlock()->getParent());
    // Builder->SetInsertPoint(DeadBB);

    return nullptr; //llvm::Constant::getNullValue(llvm::Type::getVoidTy(*TheContext));
}

llvm::Value* LabelStmtAST::codegen() {
    llvm::Function* TheFunction = Builder->GetInsertBlock()->getParent();

    // If label doesn't exist, create it
    if (!LabelTable.contains(Name)) {
        llvm::BasicBlock* LabelBB = llvm::BasicBlock::Create(*TheContext, Name, TheFunction);
        LabelTable[Name] = LabelBB;
    } else {
        // If block was created ahead of time for a goto, just insert it into function now
        if (LabelTable[Name]->getParent() == TheFunction) 
            babel_panic("Label was possibly inserted twice");

        TheFunction->insert(TheFunction->end(), LabelTable[Name]);
    }

    // Move the builder to the label
    Builder->CreateBr(LabelTable[Name]);
    Builder->SetInsertPoint(LabelTable[Name]);

    return nullptr; //llvm::Constant::getNullValue(llvm::Type::getVoidTy(*TheContext));
}

llvm::Value *BlockAST::codegen() {
    llvm::Value *Last = nullptr;
    for (const auto& Stmt : Statements) {
        Last = Stmt->codegen();
        //if (!Last)
        //    return nullptr;
    }

    return Last;
}

llvm::Value *IfStmtAST::codegen() {
    llvm::Value* CondV = Cond->codegen();
    if (!CondV)
        return nullptr;

    if (!CondV->getType()->isIntegerTy(1))
        babel_panic("Condition of if statement does not meet requirement: Boolean Type");

    if (!Builder->GetInsertBlock())
        babel_panic("InsertBlock is nullptr (global level)");

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // then block
    Builder->SetInsertPoint(ThenBB);
    Then->codegen();
    /* if (!Then->codegen())
        return nullptr; */

    Builder->CreateBr(MergeBB);
    ThenBB = Builder->GetInsertBlock();

    // else block
    TheFunction->insert(TheFunction->end(), ElseBB);
    Builder->SetInsertPoint(ElseBB);
    if (Else && !Else->codegen()) // panic instead
        return nullptr;

    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock();

    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);

    return nullptr; //llvm::Constant::getNullValue(llvm::Type::getVoidTy(*TheContext));
}

llvm::Value *TaskCallAST::codegen() {
    if (callsTo == "main") babel_panic("Calling main is not allowed, as the programs entry point it is invoked automatically");

    llvm::Function *CalleF = TheModule->getFunction(callsTo);
    if (!CalleF) babel_panic("Unknown Task '%s' referenced", callsTo.c_str());

    if (CalleF->arg_size() != Args.size()) babel_panic("Passed incorrect number of arguments (expected %d but got %d)", static_cast<int>(CalleF->arg_size()), static_cast<int>(Args.size()));

    std::vector<llvm::Value *> ArgsV;
    for (unsigned int i = 0, e = Args.size(); i != e; ++i) {
        llvm::Value *val = Args[i]->codegen();
        if (canImplicitCast(Args[i]->getType(), TaskTable.at(callsTo).args[i]))
            val = performImplicitCast(val, Args[i]->getType(), TaskTable.at(callsTo).args[i]);
        
        ArgsV.push_back(val);
        //if (!ArgsV.back()) return nullptr;
    }

    if (TaskTable.at(callsTo).ret == BabelType::Void)
        return Builder->CreateCall(CalleF, ArgsV);
    return Builder->CreateCall(CalleF, ArgsV, "calltmp");
}

llvm::Function *TaskHeaderAST::codegen() {
    std::vector<llvm::Type*> Types;
    //std::ranges::transform(ArgTypes, Types.begin(), [](const BabelType& type) { return resolveLLVMType(type); });
    for (const BabelType& type : ArgTypes) {
        Types.push_back(resolveLLVMType(type));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(resolveLLVMType(ReturnType), Types, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

    TaskTable[Name] = {ArgTypes, ReturnType};

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
    if (!TheFunction->empty()) babel_panic("Task cannot be redefined");

    llvm::IRBuilder<>::InsertPoint PrevInsertPoint = Builder->saveIP();

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    NamedValues.clear();
    //for (auto &Arg : TheFunction->args()) {
    //for (unsigned int i = 0; i < TheFunction->arg_size(); i++) {
    unsigned int i = 0;
    for (auto it = TheFunction->arg_begin(); it != TheFunction->arg_end(); it++, i++) {
        auto &Arg = *it;
        llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
        llvm::AllocaInst *Alloca = TmpB.CreateAlloca(resolveLLVMType(Header->getArgTypes()[i]), nullptr, Arg.getName());
        Builder->CreateStore(&Arg, Alloca);
        NamedValues[std::string(Arg.getName())] = {Alloca, Header->getArgTypes()[i], false};
    }

    //if (llvm::Value *RetVal = Body->codegen()) {
        //Builder->CreateRet(RetVal);
        Body->codegen();
        if (Header->getRetType() == BabelType::Void)
            Builder->CreateRetVoid();
        verifyFunction(*TheFunction);
        Builder->restoreIP(PrevInsertPoint);
        return TheFunction;
    //}

    //TheFunction->eraseFromParent();
    Builder->restoreIP(PrevInsertPoint);
    return nullptr;
}

// maybe omit BaseAST inheritance
class RootAST : public BaseAST {
    std::deque<std::unique_ptr<BaseAST>> TopLevelNodes;

    public:
        explicit RootAST(std::deque<std::unique_ptr<BaseAST>> TopLevelNodes) : TopLevelNodes(std::move(TopLevelNodes)) {}
        llvm::Function *codegen() override;
};

llvm::Function *RootAST::codegen() {
    if (llvm::Function* userDefinedMain = TheModule->getFunction("main")) {
        userDefinedMain->setName("user.main");
    }

    // Actual entry point main for libc, _start maybe later
    llvm::Type *Int32Ty = llvm::Type::getInt32Ty(*TheContext);
    llvm::PointerType *CharPtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*TheContext));
    llvm::FunctionType *MainType = llvm::FunctionType::get(Int32Ty, {Int32Ty, CharPtrTy, CharPtrTy}, false);

    llvm::Function *MainFn = llvm::Function::Create(MainType, llvm::Function::ExternalLinkage, "main", TheModule.get());
    llvm::BasicBlock *MainEntry = llvm::BasicBlock::Create(*TheContext, "entry", MainFn);
    Builder->SetInsertPoint(MainEntry);

    auto ArgIter = MainFn->arg_begin();
    llvm::Value *Argc = &*ArgIter++;
    llvm::Value *Argv = &*ArgIter++;
    llvm::Value *Envp = &*ArgIter++;

    // Store globals
    auto *GArgc = new llvm::GlobalVariable(
        *TheModule, Int32Ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, "__argc__"
    );
    GArgc->setInitializer(llvm::ConstantInt::get(Int32Ty, 0));

    auto *GArgv = new llvm::GlobalVariable(
        *TheModule, CharPtrTy, false, llvm::GlobalValue::ExternalLinkage, nullptr, "__argv__"
    );
    GArgv->setInitializer(llvm::ConstantPointerNull::get(CharPtrTy));

    auto *GEnvp = new llvm::GlobalVariable(
        *TheModule, CharPtrTy, false, llvm::GlobalValue::ExternalLinkage, nullptr, "__envp__"
    );
    GEnvp->setInitializer(llvm::ConstantPointerNull::get(CharPtrTy));

    Builder->CreateStore(Argc, GArgc);
    Builder->CreateStore(Argv, GArgv);
    Builder->CreateStore(Envp, GEnvp);

    GlobalValues["__argc__"] = {GArgc, BabelType::Int32, false};
    GlobalValues["__argv__"] = {GArgv, BabelType::CString, false};
    GlobalValues["__envp__"] = {GEnvp, BabelType::CString, false};

    // __global_main wrapper for top level code
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext), false);
    llvm::Function *globalMain = llvm::Function::Create(FT, llvm::Function::InternalLinkage, "__global_main", TheModule.get());

    llvm::BasicBlock *Entry = llvm::BasicBlock::Create(*TheContext, "entry", globalMain);
    Builder->SetInsertPoint(Entry);

    for (const auto& Node : TopLevelNodes) {
        Node->codegen();
    }

    if (llvm::Function *UserMain = TheModule->getFunction("user.main")) {
        if (UserMain->getReturnType()->isVoidTy()) {
            Builder->CreateCall(UserMain);
            Builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0));
        } else if (UserMain->getReturnType()->isIntegerTy(32)) {
            llvm::Value* RetVal = Builder->CreateCall(UserMain);
            Builder->CreateRet(RetVal);
        } else {
            babel_panic("main method must return integer or void type");
        }
    } else {
        Builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0));
    }

    // Call __global_main and return its result
    Builder->SetInsertPoint(MainEntry);
    llvm::Value *GlobalMainRet = Builder->CreateCall(globalMain);
    Builder->CreateRet(GlobalMainRet);

    return MainFn;
}