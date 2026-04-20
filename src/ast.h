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
#include <numeric>
#include <string>
#include <map>
#include <iostream>
#include <variant>
#include <vector>
#include <algorithm>

#include "util.hpp"
#include "typing.h"

#define GLOBAL_SCOPE Builder->GetInsertBlock()->getParent()->getName().starts_with("__global_main")

struct GlobalSymbol {
    llvm::GlobalVariable* val;
    BabelType type;
    bool isConstant;
    bool isComptime; // we might need to change this in the future, in case the variable is overridden e.g. let s = "Hello World"; s = input("Enter a sring: ")
    llvm::Constant* comptimeInit;
};

struct LocalSymbol {
    llvm::AllocaInst* val;
    BabelType type;
    bool isConstant;
};

struct TaskTypeInfo {
    std::deque<BabelType> args;
    BabelType ret;
    bool isVarArg;
};

struct LoopInfo {
    llvm::BasicBlock* __continue__;
    llvm::BasicBlock* __break__;
};

// static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
// static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, LocalSymbol> NamedValues;
static std::map<std::string, GlobalSymbol> GlobalValues;
static std::map<std::string, llvm::BasicBlock*> LabelTable;
static std::map<std::string, LoopInfo> LoopTable = {{".active", {nullptr, nullptr}}};
static std::map<std::string, TaskTypeInfo> TaskTable;
static std::map<std::string, bool> PolymorphTable;

// Base class for all expression node
class BaseAST {
    public:
        virtual ~BaseAST() = default;
        virtual llvm::Value *codegen() = 0;
        virtual llvm::Constant *codegenComptime() { babel_panic("Cannot generate value at compile time"); }
        virtual llvm::Value *requireLValue() { babel_panic("No lvalue available for this AST node"); }
        virtual BabelType getType() const { babel_panic("getType() not supported for this AST node"); }
        virtual bool isComptimeAssignable() const { babel_panic("isComptimeAssignable() not supported for this AST node"); }
        virtual bool isStatementLike() const { return false; };
};

// class for referencing variables
class VariableAST : public BaseAST {
    const std::string Name;
    const std::optional<BabelType> Type;
    bool isConst;
    const bool isDecl;
    bool isComptime;
    bool requiresLValue = false;

    public:
        VariableAST(const std::string &Name, const std::optional<BabelType>& Type, const bool isConst, const bool isDecl, const bool isComptime) : Name(Name), Type(Type), isConst(isConst), isDecl(isDecl), isComptime(isComptime) {
           if (GlobalValues.contains(Name)) { this->isConst = GlobalValues.at(Name).isConstant; this->isComptime = GlobalValues.at(Name).isComptime; }
           else if (Type.has_value()) { insertSymbol(); }
            /*  if (isDecl) { insertSymbol(); }
            // else { this->isConst = GlobalValues.at(Name).isConstant; this->isComptime = GlobalValues.at(Name).isComptime; }
            else if (GlobalValues.contains(Name)) { this->isConst = GlobalValues.at(Name).isConstant; this->isComptime = GlobalValues.at(Name).isComptime; }
            // TODO: each variable auto updates itself so the node knows wether its variable is constant or not
            // this allows for a significant simplification of the handleAssignment method */
        }
        void insertSymbol() const;
        static void insertSymbol(const std::string& Name, BabelType type, const bool isConst, const bool isComptime);
        std::string getName() const { return Name; }
        bool getConstness() const { return isConst; }
        bool getDecl() const { return isDecl; }
        bool hasComptimeVal() const { return isComptime; }
        BabelType getType() const override;
        bool isComptimeAssignable() const override { return GlobalValues.at(Name).isComptime; }
        llvm::Value *codegen() override;
        llvm::Constant *codegenComptime() override;
        llvm::Value *requireLValue() override {
            requiresLValue = true;
            llvm::Value* lVal = codegen();
            requiresLValue = false;
            assert(lVal->getType()->isPointerTy() && "requireLValue returned non-pointer");
            return lVal;
        }
};

class BooleanAST : public BaseAST {
    const int Val;

    public:
        explicit BooleanAST(std::string_view value) : Val(value == "TRUE" ? 1 : 0) {}
        llvm::Value *codegen() override { return codegenComptime(); }
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return BabelType::Boolean(); }
        bool isComptimeAssignable() const override { return true; }
};

// class for numeric literals which are integers
class IntegerAST : public BaseAST {
    llvm::APInt Val;
    BabelType Type;

    public:
        friend class FloatingPointAST;
        explicit IntegerAST(std::string& s) {
            if (s.find("''") != std::string::npos)
                babel_panic("adjacent digit separators");

            std::erase(s, '\'');

            if (s.starts_with("0x")) {
                if (s.find_first_of("SsIiLl", 2) != std::string::npos && s.at(s.size() - 2) != '_')
                    babel_panic("invalid hex literal: type suffix requires _ as a separator");
                
                std::tie(Val, Type) = parseInt(s, 2, 16);
            } else if (s.starts_with("0o")) {
                if (s.find_first_of("89AaDdEeFf", 2) != std::string::npos || (s.find_first_of("BbCc", 2) != std::string::npos && s.find_first_of("BbCc", 2) != s.size() - 1))
                    babel_panic("invalid octal literal: only digits 0-7 are allowed");

                std::tie(Val, Type) = parseInt(s, 2, 8);
            } else if (s.starts_with("0b")) {
                if (s.find_first_of("23456789AaDdEeFf", 2) != std::string::npos || (s.find_first_of("BbCc", 2) != std::string::npos && s.find_first_of("BbCc", 2) != s.size() - 1))
                    babel_panic("invalid binary literal: only digits 0 and 1 are allowed");

                std::tie(Val, Type) = parseInt(s, 2, 2);
            } else {
                if (s.find_first_of("AaDdEeFf") != std::string::npos || (s.find_first_of("BbCc") != std::string::npos && s.find_first_of("BbCc") != s.size() - 1))
                    babel_panic("invalid decimal literal: only digits 0-9 are allowed");

                std::tie(Val, Type) = parseInt(s, 0, 10);
            }
        }
        llvm::Value *codegen() override { return codegenComptime(); }
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return Type; }
        bool isComptimeAssignable() const override { return true; }
};

class CharacterAST : public BaseAST {
    const char Val;

    public:
        explicit CharacterAST(const char Val) : Val(Val) {}
        llvm::Value *codegen() override { return codegenComptime(); }
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return BabelType::Character(); }
        bool isComptimeAssignable() const override { return true; }
};

class CStringAST : public BaseAST {
    const std::string Val;

    public:
        explicit CStringAST(const std::string& Val) : Val(Val) {}
        llvm::Value *codegen() override { return codegenComptime(); }
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return BabelType::CString(); }
        bool isComptimeAssignable() const override { return true; }
};

// class for numeric literals which are floating points
class FloatingPointAST : public BaseAST {
    llvm::APFloat Val = llvm::APFloat(0.0);
    BabelType Type;

    public:
        explicit FloatingPointAST(std::string& s) {
            if (s.find("''") != std::string::npos)
                babel_panic("adjacent digit separators");
            
            std::erase(s, '\'');

            bool isGenuineFP = s.find_first_of(".EePp") != std::string::npos || s == "NaN" || s == "Inf";
            bool isHexFloat = s.starts_with("0x");

            if (!isGenuineFP) {
                // this is an integer with type suffix
                if (isHexFloat && s.find('_') == std::string::npos)
                    babel_panic("invalid hex literal: type suffix requires _ as a separator");

                std::string fSuffix = "HFDQ";
                std::string iSuffix = "SILC";
                
                char suffix = s.back();
                s.back() = iSuffix.at(fSuffix.find(static_cast<char>(toupper(suffix))));

                auto Int = IntegerAST(s);
                Val = llvm::APFloat(fpSemanticsFromSuffix(suffix), Int.Val);
                Type = fpTypeFromSuffix(suffix);
            } else {
                if (isHexFloat && s.find_first_of("Pp") == std::string::npos)
                    babel_panic("hex float must contain an exponent");

                std::size_t len = s.size();
                char suffix = '\0';
                if (s.find("_") != std::string::npos) {
                    len -= 2; suffix = s.back();
                } else if (!isHexFloat && s != "Inf" && s.find_first_of("HhFfDdQq") != std::string::npos) {
                    len--; suffix = s.back();
                }

                // We use Inf to indicate infinity, llvm however accepts only lowercase versions
                std::ranges::transform(s, s.begin(), [](unsigned char c){ return std::tolower(c); });
                
                Val = llvm::APFloat(fpSemanticsFromSuffix(suffix), s.substr(0, len));
                Type = fpTypeFromSuffix(suffix);
            }
        }
        llvm::Value *codegen() override { return codegenComptime(); }
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return Type; }
        bool isComptimeAssignable() const override { return true; }
};

class ArrayAST : public BaseAST {
    const std::deque<std::unique_ptr<BaseAST>> Val;
    const size_t Size;
    const BabelType Inner;

    public:
        // arbitrary type for empty arrays
        explicit ArrayAST(std::deque<std::unique_ptr<BaseAST>> _Val) : Val(std::move(_Val)), Size(Val.size()), Inner(Size > 0 ? Val.front()->getType() : BabelType::Int()) {
            // TODO: type casting should be allowed, e.g. Array(1, 2.9, 4)
            for (const auto& elmnt : Val) {
                if (Inner != elmnt->getType())
                    babel_panic("Array elements must share the same type");
            }
        }
        llvm::Value* codegen() override;
        llvm::Constant *codegenComptime() override;
        BabelType getType() const override { return BabelType::Array(&Inner, Size); }
        bool isComptimeAssignable() const override { return std::ranges::all_of(Val, [](const std::unique_ptr<BaseAST>& elmnt) {return elmnt->isComptimeAssignable(); }); }
};

class AccessElementOperatorAST : public BaseAST {
    std::unique_ptr<BaseAST> Container;
    std::unique_ptr<BaseAST> Index;
    bool requiresLValue = false;

    public:
        AccessElementOperatorAST(std::unique_ptr<BaseAST> Container, std::unique_ptr<BaseAST> Index) : Container(std::move(Container)), Index(std::move(Index)) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return *(Container->getType().getArray().inner); }
        bool isComptimeAssignable() const override { return false; }
        llvm::Value *requireLValue() override {
            requiresLValue = true;
            llvm::Value* lVal = codegen();
            requiresLValue = false;
            assert(lVal->getType()->isPointerTy() && "requireLValue returned non-pointer");
            return lVal;
        }
};

class DereferenceOperatorAST : public BaseAST {
    std::unique_ptr<BaseAST> Var;
    bool requiresLValue = false;

    public:
        explicit DereferenceOperatorAST(std::unique_ptr<BaseAST> Var) : Var(std::move(Var)) {}
        llvm::Value *codegen() override;
        BabelType getType() const override { return *(Var->getType().getPointer().to); }
        bool isComptimeAssignable() const override { return false; }
        llvm::Value *requireLValue() override {
            requiresLValue = true;
            llvm::Value* lVal = codegen();
            requiresLValue = false;
            return lVal;
        }
};

class AddressOfOperatorAST : public BaseAST {
    std::unique_ptr<VariableAST> Var;
    const BabelType To;

    public:
        explicit AddressOfOperatorAST(std::unique_ptr<BaseAST> _Var) : To(_Var->getType()) {
            BaseAST* const stored_ptr = _Var.release();
            Var = std::unique_ptr<VariableAST>(dynamic_cast<VariableAST*>(stored_ptr));
            if (!Var) {
                _Var.reset(stored_ptr);
                babel_panic("Cannot create pointer from non-variable");
            }
        }
        llvm::Value *codegen() override;
        llvm::Constant *codegenComptime() override { assert(isComptimeAssignable()); return llvm::cast<llvm::Constant>(codegen()); }
        BabelType getType() const override { return BabelType::Pointer(&To, Var->getConstness()); }
        bool isComptimeAssignable() const override { return Var->isComptimeAssignable(); }
};

class ComparisonChainAST : public BaseAST {
    const std::deque<std::string> Operators;
    const std::deque<std::unique_ptr<BaseAST>> Operands;

    public:
        ComparisonChainAST(std::deque<std::string> Operators, std::deque<std::unique_ptr<BaseAST>> Operands) : Operators(std::move(Operators)), Operands(std::move(Operands)) {}
        llvm::Value *codegen() override;
        llvm::Constant *codegenComptime() override { assert(isComptimeAssignable()); return llvm::cast<llvm::Constant>(codegen()); }
        BabelType getType() const override { return BabelType::Boolean(); }
        bool isComptimeAssignable() const override { return std::ranges::all_of(Operands, [](const std::unique_ptr<BaseAST>& elmnt) { return elmnt->isComptimeAssignable(); }); }
        bool isStatementLike() const override { return false; }
};

// class for when binary operators are used
class BinaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> LHS;
    std::unique_ptr<BaseAST> RHS;

    public:
        BinaryOperatorAST(const std::string& Op, std::unique_ptr<BaseAST> LHS, std::unique_ptr<BaseAST> RHS) : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        llvm::Value *codegen() override;
        llvm::Constant* codegenComptime() override { assert(isComptimeAssignable()); return llvm::cast<llvm::Constant>(codegen()); }
        BabelType getType() const override;
        bool isComptimeAssignable() const override { return LHS->isComptimeAssignable() && RHS->isComptimeAssignable(); }
        bool isStatementLike() const override { return Op == "=" || Op == ":="; }
};

class UnaryOperatorAST : public BaseAST {
    const std::string Op;
    std::unique_ptr<BaseAST> Val;

    public:
        UnaryOperatorAST(const std::string& Op, std::unique_ptr<BaseAST> Val) : Op(Op), Val(std::move(Val)) {}
        llvm::Value *codegen() override;
        llvm::Constant* codegenComptime() override { assert(isComptimeAssignable()); return llvm::cast<llvm::Constant>(codegen()); }
        BabelType getType() const override { return Val->getType(); }
        bool isComptimeAssignable() const override { return Val->isComptimeAssignable(); }
        bool isStatementLike() const override { return Op.ends_with("++") || Op.ends_with("--"); }
};

class ContinueStmtAST : public BaseAST {
    std::optional<std::string> Target;

    public:
        explicit ContinueStmtAST(const std::optional<std::string>& Target) : Target(Target) {}
        llvm::Value *codegen() override;
};

class BreakStmtAST : public BaseAST {
    std::optional<std::string> Target;

    public:
        explicit BreakStmtAST(const std::optional<std::string>& Target) : Target(Target) {}
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
        const std::string &getName() const { return Name; }
        llvm::Value *codegen() override;
};

class BlockAST : public BaseAST {
    std::deque<std::unique_ptr<BaseAST>> Statements;

    public:
        explicit BlockAST(std::deque<std::unique_ptr<BaseAST>> Statements) : Statements(std::move(Statements)) {}
        llvm::Value *codegen() override;
        bool isStatementLike() const override { return Statements.empty(); }
};

class IfStmtAST : public BaseAST {
    std::unique_ptr<BaseAST> Cond;
    std::unique_ptr<BaseAST> Then;
    std::unique_ptr<BaseAST> Else;

    public:
        IfStmtAST(std::unique_ptr<BaseAST> Cond, std::unique_ptr<BaseAST> Then, std::unique_ptr<BaseAST> Else) : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
        llvm::Value *codegen() override;
};

class WhileLoopAST : public BaseAST {
    std::optional<std::string> Label;
    std::unique_ptr<BaseAST> Cond;
    std::unique_ptr<BaseAST> Body;

    public:
        WhileLoopAST(const std::optional<std::string>& Label, std::unique_ptr<BaseAST> Cond, std::unique_ptr<BaseAST> Body) : Label(Label), Cond(std::move(Cond)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
};

class ForLoopAST : public BaseAST {
    std::optional<std::string> Label;
    std::unique_ptr<BaseAST> Init;
    std::unique_ptr<BaseAST> Cond;
    std::unique_ptr<BaseAST> Update;
    std::unique_ptr<BaseAST> Body;

    public:
        ForLoopAST(const std::optional<std::string>& Label, std::unique_ptr<BaseAST> Init, std::unique_ptr<BaseAST> Cond, std::unique_ptr<BaseAST> Update, std::unique_ptr<BaseAST> Body) : Label(Label), Init(std::move(Init)), Cond(std::move(Cond)), Update(std::move(Update)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
};

class ForInLoopAST : public BaseAST {
    std::optional<std::string> Label;
    std::unique_ptr<BaseAST> Elmnt;
    std::unique_ptr<BaseAST> Collection;
    std::unique_ptr<BaseAST> Body;

    public:
        ForInLoopAST(const std::optional<std::string>& Label, std::unique_ptr<BaseAST> Elmnt, std::unique_ptr<BaseAST> Collection, std::unique_ptr<BaseAST> Body) : Label(Label), Elmnt(std::move(Elmnt)), Collection(std::move(Collection)), Body(std::move(Body)) {}
        llvm::Value *codegen() override;
};

class MacroCallAST : public BaseAST {
    std::string name;
    std::deque<std::variant<std::unique_ptr<BaseAST>, BabelType>> Args;

    public:
        MacroCallAST(const std::string& name, std::deque<std::variant<std::unique_ptr<BaseAST>, BabelType>> Args) : name(name), Args(std::move(Args)) {}
        BabelType getType() const override;
        llvm::Value *codegen() override;
        bool isComptimeAssignable() const override { return true; }
        bool isStatementLike() const override { return true; }
};

// class for when a function is called
class TaskCallAST : public BaseAST {
    std::string callsTo;
    std::deque<std::unique_ptr<BaseAST>> Args;

    public:
        TaskCallAST(const std::string &callsTo, std::deque<std::unique_ptr<BaseAST>> Args) : callsTo(callsTo), Args(std::move(Args)) {}
        BabelType getType() const override;
        llvm::Value *codegen() override;
        bool isComptimeAssignable() const override { return false; } /* true if it's comptime, but that doesn't exist yet */
        bool isStatementLike() const override { return true; }
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    std::string Name;
    std::deque<std::string> Args;
    std::deque<BabelType> ArgTypes;
    BabelType ReturnType;
    bool isVarArg;

    public:
        TaskHeaderAST(const std::string &Name, std::deque<std::string> Args, std::deque<BabelType> ArgTypes, BabelType ReturnType, bool isVarArg) : Name(Name), Args(std::move(Args)), ArgTypes(std::move(ArgTypes)), ReturnType(ReturnType), isVarArg(isVarArg) {
            TaskTable[Name] = {this->ArgTypes, ReturnType, isVarArg};
            PolymorphTable[Name] = PolymorphTable.contains(Name);

            for (size_t i = 0; i < Args.size(); i++) {
                VariableAST::insertSymbol(Args[i], ArgTypes[i], false, false);
            }
        }
        llvm::Function *codegen() override;
        const std::string &getName() const { return Name; }
        const std::deque<BabelType> &getArgTypes() const { return ArgTypes; }
        const BabelType &getRetType() const { return ReturnType; }
        void update() {
            if (PolymorphTable.contains(Name) && PolymorphTable.at(Name)) {
                auto underscore_fold = [](std::string a, BabelType b) { return std::move(a) + '_' + getBabelTypeName(b); };
 
                std::string typeinfo = !ArgTypes.empty() ? std::accumulate(std::next(ArgTypes.begin()), ArgTypes.end(), getBabelTypeName(ArgTypes[0]), underscore_fold) : "";

                auto node_handle = TaskTable.extract(Name);
                Name = std::format("{}.polymorphic.{}", Name, typeinfo);
                Name += isVarArg ? "_..." : "";
                
                if (!node_handle.empty()) {
                    node_handle.key() = Name;
                    node_handle.mapped() = {ArgTypes, ReturnType, isVarArg};
                    TaskTable.insert(std::move(node_handle));
                } else {
                    TaskTable[Name] = {ArgTypes, ReturnType, isVarArg};
                }
            }
        }
};

// class for the function definition
class TaskAST : public BaseAST {
    std::unique_ptr<TaskHeaderAST> Header;
    std::unique_ptr<BaseAST> Body;

    public:
        TaskAST(std::unique_ptr<TaskHeaderAST> Header, std::unique_ptr<BaseAST> Body) : Header(std::move(Header)), Body(std::move(Body)) {}
        llvm::Function *codegen() override;
};

void VariableAST::insertSymbol() const {
    // nullptr is not viewed as having a value
    // since we can't check using the Builder, just insert into both
    GlobalValues[Name] = {nullptr, Type.value(), isConst, isComptime, nullptr};
    NamedValues[Name] = {nullptr, Type.value(), isConst};
}

void VariableAST::insertSymbol(const std::string& Name, BabelType type, const bool isConst, const bool isComptime) {
    GlobalValues[Name] = {nullptr, type, isConst, isComptime, nullptr};
    NamedValues[Name] = {nullptr, type, isConst};
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
    BabelType lTy = LHS->getType();
    BabelType rTy = RHS->getType();

    using enum OpKind;
    switch (getOperation(Op, lTy, rTy)) {
        case AddPtr: case SubPtr:
            return lTy.isPointer() ? lTy : rTy;
        case PtrDiff:
            return BabelType::Int(); // should be ptr sized int in the future
        case MulBool:
            return lTy != BabelType::Boolean() ? lTy : rTy;
        case PowerInt:
            return BabelType::Float64();
        case Div: {
            if (isBabelInteger(lTy) && isBabelInteger(rTy))
                return BabelType::Float64();
            
            /* fallthrough */
        }
        case AddInt: case AddFloat: case SubInt: case SubFloat:
        case MulInt: case MulFloat: case IDiv: case RemInt:
        case RemFloat: case Shl: case Shr: case LShr:
        case PowerFloatInt: case PowerFloat: case BitAnd: case BitXor:
        case BitOr: {
            if (canImplicitCast(lTy, rTy)) {
                return rTy;
            } else if (canImplicitCast(rTy, lTy)) {
                return lTy;
            }
        }
        default:
            babel_unreachable();
    }
}

BabelType TaskCallAST::getType() const  {
    return TaskTable.at(callsTo).ret;
}

void StoreOrMemCpy(BaseAST* src, BabelType srcType, llvm::Value* dest, BabelType destType) {
    // Aggregate would be more precise, change this in the future
    if (src->getType().isArray()) {
        llvm::Type* type = resolveLLVMType(src->getType());
        uint64_t size = TheModule->getDataLayout().getTypeAllocSize(type);
        llvm::Align align = TheModule->getDataLayout().getABITypeAlign(type);
        if (auto* SRC = dynamic_cast<VariableAST*>(src)) {
            Builder->CreateMemCpy(dest, align, SRC->requireLValue(), align, size);
            return;
        }
        
        llvm::Value* srcVal = src->codegen();
        if (canImplicitCast(srcType, destType))
            srcVal = performImplicitCast(srcVal, srcType, destType);

        Builder->CreateMemCpy(dest, align, srcVal, align, size);
    } else {
        llvm::Value* srcVal = src->codegen();
        if (canImplicitCast(srcType, destType))
                srcVal = performImplicitCast(srcVal, srcType, destType);

        Builder->CreateStore(srcVal, dest);
    }
}

llvm::Value *handleAssignment(BaseAST* RHS, BabelType RHSType, BabelType VarType, const std::string& VarName, const bool isConst, const bool isDeclaration, const bool isComptime, const bool isShortDecl) {
    if (GLOBAL_SCOPE) {
        // we are in global scope

        if (GlobalValues.contains(VarName) && GlobalValues.at(VarName).val != nullptr) {
            if (isDeclaration && !isShortDecl) {
                babel_panic("Redefinition of global variable '%s'", VarName.c_str());
            }

            GlobalSymbol& existing = GlobalValues[VarName];
            if (existing.isConstant) {
                babel_panic("Cannot assign to constant '%s'", VarName.c_str());
            }

            StoreOrMemCpy(RHS, RHSType, existing.val, existing.type);
            return existing.val;
        }

        if (!isDeclaration && !isShortDecl) {
            babel_panic("Variable '%s' used before declaration", VarName.c_str());
        }

        llvm::Constant* zeroInit = llvm::Constant::getNullValue(resolveLLVMType(VarType));
        llvm::Constant* initializer = isComptime ? llvm::cast<llvm::Constant>(performImplicitCast(RHS->codegenComptime(), RHSType, VarType)) : zeroInit;

        auto *GV = new llvm::GlobalVariable(
            *TheModule,
            resolveLLVMType(VarType),
            isConst,
            llvm::GlobalValue::ExternalLinkage,
            initializer,
            VarName
        );

        if (!isComptime) {
            StoreOrMemCpy(RHS, RHSType, GV, VarType);
            // If not in "script mode":
            // babel_panic("Global variables must be initialized with constant values");
        }

        GlobalValues[VarName] = {GV, VarType, isConst, isComptime, initializer};
        return GV;
    } else {
        // we are in local scope
        LocalSymbol Var = NamedValues[VarName];

        if (!Var.val) {
            if (GlobalValues.contains(VarName) && GlobalValues.at(VarName).val != nullptr) {
                if (isDeclaration && !isShortDecl) {
                    babel_panic("Redefinition of global variable '%s'", VarName.c_str());
                }
    
                GlobalSymbol& existing = GlobalValues[VarName];
                if (existing.isConstant) {
                    babel_panic("Cannot assign to constant '%s'", VarName.c_str());
                }

                StoreOrMemCpy(RHS, RHSType, existing.val, existing.type);
                return existing.val;
            }

            if (!isDeclaration && !isShortDecl) {
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

        StoreOrMemCpy(RHS, RHSType, Var.val, Var.type);
        return Var.val;
    }
}

llvm::Value *ArrayAST::codegen() {
    llvm::ArrayType* type = llvm::ArrayType::get(resolveLLVMType(Inner), Size);

    llvm::Value* ptr = Builder->CreateAlloca(type);
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);

    for (int i = 0; i < Val.size(); i++) {
        llvm::Value* index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), i);
        llvm::Value* slot = Builder->CreateGEP(type, ptr, {zero, index});
        StoreOrMemCpy(Val[i].get(), Val[i]->getType(), slot, Inner);
    }

    return ptr;
}

llvm::Constant *ArrayAST::codegenComptime() {
    llvm::ArrayType* type = llvm::ArrayType::get(resolveLLVMType(Inner), Size);

    assert(isComptimeAssignable() && GLOBAL_SCOPE);

    std::vector<llvm::Constant*> Args;
    for (const auto& elmnt : Val) {
        auto* var = dynamic_cast<VariableAST*>(elmnt.get());
        Args.push_back(var == nullptr ? elmnt->codegenComptime() : GlobalValues.at(var->getName()).comptimeInit);
    }

    return llvm::ConstantArray::get(type, Args);
}

llvm::Constant *FloatingPointAST::codegenComptime() {
    return llvm::ConstantFP::get(resolveLLVMType(Type), Val);
}

llvm::Constant *IntegerAST::codegenComptime() {
    return llvm::ConstantInt::get(resolveLLVMType(Type), Val);
}

llvm::Constant *CharacterAST::codegenComptime() {
    return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*TheContext), Val);
}

llvm::Constant *CStringAST::codegenComptime() {
    return Builder->CreateGlobalString(Val, ".cstr");
}

llvm::Constant *BooleanAST::codegenComptime() {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*TheContext), Val);
}

llvm::Value *VariableAST::codegen() {
    if (NamedValues.contains(Name) && NamedValues.at(Name).val != nullptr) {
        if (requiresLValue)
            return NamedValues[Name].val;
        
        return Builder->CreateLoad(resolveLLVMType(NamedValues[Name].type), NamedValues[Name].val, Name);
    } else if (GlobalValues.contains(Name) && GlobalValues.at(Name).val != nullptr) {
        if (requiresLValue)
            return GlobalValues[Name].val;
        
        return Builder->CreateLoad(resolveLLVMType(GlobalValues[Name].type), GlobalValues[Name].val, Name);
    } else {
        babel_panic("Unknown variable '%s' referenced", Name.c_str());
    }
}

llvm::Constant *VariableAST::codegenComptime() {
    assert(isComptimeAssignable());
    return GlobalValues.at(Name).comptimeInit;
}

llvm::Value *shortCircuit(const std::deque<std::unique_ptr<BaseAST>>& ops, bool continueCondition) {
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(*TheContext, "L", TheFunction);
    std::vector<llvm::BasicBlock*> blocks = {Builder->GetInsertBlock()};

    for (const auto& op : ops | std::views::take(ops.size() - 1)) {
        llvm::BasicBlock *ContinueBB = llvm::BasicBlock::Create(*TheContext, "L", TheFunction, EndBB);
        blocks.push_back(ContinueBB);

        continueCondition ? Builder->CreateCondBr(op->codegen(), ContinueBB, EndBB) : Builder->CreateCondBr(op->codegen(), EndBB, ContinueBB);
        Builder->SetInsertPoint(ContinueBB);
    }

    llvm::Value* lastVal = ops.back()->codegen();
    Builder->CreateBr(EndBB);

    Builder->SetInsertPoint(EndBB);
    // llvm::PHINode *phi = llvm::PHINode::Create(llvm::Type::getInt1Ty(*TheContext), static_cast<unsigned>(blocks.size()));
    llvm::PHINode* phi = Builder->CreatePHI(llvm::Type::getInt1Ty(*TheContext), static_cast<unsigned>(blocks.size()));

    for (const auto& block : blocks | std::views::take(blocks.size() - 1)) {
        llvm::Value *b = continueCondition ? llvm::ConstantInt::getFalse(*TheContext) : llvm::ConstantInt::getTrue(*TheContext);
        phi->addIncoming(b, block);
    }

    phi->addIncoming(lastVal, blocks.back());
    return phi;
}

llvm::Value *cmpHelper(OpKind op, llvm::Value *lhs, llvm::Value *rhs) {
    using enum OpKind;
    switch (op) {
        case EqInt:
            return Builder->CreateICmpEQ(lhs, rhs, "eqtmp");
        case EqFloat:
            return Builder->CreateFCmpUEQ(lhs, rhs, "eqtmp");
        case NeInt:
            return Builder->CreateICmpNE(lhs, rhs, "netmp");
        case NeFloat:
            return Builder->CreateFCmpUNE(lhs, rhs, "netmp");
        case LtInt:
            return Builder->CreateICmpSLT(lhs, rhs, "lttmp");
        case LtFloat:
            return Builder->CreateFCmpULT(lhs, rhs, "lttmp");
        case LeInt:
            return Builder->CreateICmpSLE(lhs, rhs, "letmp");
        case LeFloat:
            return Builder->CreateFCmpULE(lhs, rhs, "letmp");
        case GtInt:
            return Builder->CreateICmpSGT(lhs, rhs, "gttmp");
        case GtFloat:
            return Builder->CreateFCmpUGT(lhs, rhs, "gttmp");
        case GeInt:
            return Builder->CreateICmpSGE(lhs, rhs, "getmp");
        case GeFloat:
            return Builder->CreateFCmpUGE(lhs, rhs, "getmp");
        
        default:
            babel_unreachable();
    }
}

llvm::Value *ComparisonChainAST::codegen() {
    assert(Operators.size() == Operands.size() - 1);

    if (std::ranges::all_of(Operators, [](std::string_view op){ return op == "&&"; }))
        return shortCircuit(Operands, true);

    if (std::ranges::all_of(Operators, [](std::string_view op){ return op == "||"; }))
        return shortCircuit(Operands, false);

    assert(std::ranges::all_of(Operators, [](const std::string& op){ return op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!="; }));

    if (Operands.size() == 2)
        return cmpHelper(getOperation(Operators.front(), Operands[0]->getType(), Operands[1]->getType()), Operands[0]->codegen(), Operands[1]->codegen());

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(*TheContext, "L", TheFunction);
    std::vector<llvm::BasicBlock*> blocks = {Builder->GetInsertBlock()};
    llvm::Value *left = Operands.front()->codegen();
    BabelType lTy = Operands.front()->getType();

    for (const auto&[op, o] : Zipped{Operands | std::views::drop(1) | std::views::take(Operands.size() - 1), Operators | std::views::take(Operators.size() - 1)}) {
        llvm::BasicBlock *ContinueBB = llvm::BasicBlock::Create(*TheContext, "L", TheFunction, EndBB);
        blocks.push_back(ContinueBB);

        llvm::Value* right = op->codegen();
        BabelType rTy = op->getType();

        Builder->CreateCondBr(cmpHelper(getOperation(o, lTy, rTy), left, right), ContinueBB, EndBB);
        Builder->SetInsertPoint(ContinueBB);

        left = right;
        lTy = rTy;
    }

    llvm::Value* lastVal = cmpHelper(getOperation(Operators.back(), lTy, Operands.back()->getType()), left, Operands.back()->codegen());
    Builder->CreateBr(EndBB);

    Builder->SetInsertPoint(EndBB);
    llvm::PHINode *phi = Builder->CreatePHI(llvm::Type::getInt1Ty(*TheContext), static_cast<unsigned>(blocks.size()));

    for (const auto& block : blocks | std::views::take(blocks.size() - 1)) {
        phi->addIncoming(llvm::ConstantInt::getFalse(*TheContext), block);
    }

    phi->addIncoming(lastVal, blocks.back());
    return phi;
}

llvm::Function *getOrCreate_ipow(llvm::Type* ty) {
    std::string name = std::format("babel.ipow.i{}.i{}", ty->getIntegerBitWidth(), ty->getIntegerBitWidth());

    llvm::Function* F = TheModule->getFunction(name);
    if (F) return F;

    llvm::FunctionType *FT = llvm::FunctionType::get(Builder->getDoubleTy(), {ty, ty}, false);
    F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, TheModule.get());
    F->getArg(0)->setName("Val"); F->getArg(1)->setName("Power");

    llvm::IRBuilder<>::InsertPoint PrevInsertPoint = Builder->saveIP();

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);

    llvm::IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
    llvm::AllocaInst *Alloca0 = TmpB.CreateAlloca(ty);
    llvm::AllocaInst *Alloca1 = TmpB.CreateAlloca(ty);
    Builder->CreateStore(F->getArg(0), Alloca0);
    Builder->CreateStore(F->getArg(1), Alloca1);
    
    llvm::Value* power = Builder->CreateLoad(ty, Alloca1);
    llvm::Value* eq = Builder->CreateICmpEQ(power, llvm::ConstantInt::get(ty, 0));

    llvm::BasicBlock* BaseBB = llvm::BasicBlock::Create(*TheContext, "", F);
    llvm::BasicBlock* RecBB = llvm::BasicBlock::Create(*TheContext, "", F);
    llvm::BasicBlock* IPowBB = llvm::BasicBlock::Create(*TheContext, "", F);
    llvm::BasicBlock* UPowBB = llvm::BasicBlock::Create(*TheContext, "", F);
    Builder->CreateCondBr(eq, BaseBB, RecBB);

    Builder->SetInsertPoint(BaseBB);
    Builder->CreateRet(llvm::ConstantFP::get(Builder->getDoubleTy(), 1));

    Builder->SetInsertPoint(RecBB);
    llvm::Value* lt = Builder->CreateICmpSLT(Builder->CreateLoad(ty, Alloca1), llvm::ConstantInt::get(ty, 0));
    Builder->CreateCondBr(lt, IPowBB, UPowBB);

    Builder->SetInsertPoint(IPowBB);
    llvm::Value* neg = Builder->CreateNeg(Builder->CreateLoad(ty, Alloca1));
    llvm::Value* calltmp = Builder->CreateCall(F, {Builder->CreateLoad(ty, Alloca0), neg});
    llvm::Value* div = Builder->CreateFDiv(llvm::ConstantFP::get(Builder->getDoubleTy(), 1), calltmp);
    Builder->CreateRet(div);
    
    Builder->SetInsertPoint(UPowBB);
    llvm::Value* sub = Builder->CreateSub(Builder->CreateLoad(ty, Alloca1), llvm::ConstantInt::get(ty, 1));
    llvm::Value* calltmp1 = Builder->CreateCall(F, {Builder->CreateLoad(ty, Alloca0), sub});
    llvm::Value* mul = Builder->CreateMul(Builder->CreateSIToFP(Builder->CreateLoad(ty, Alloca0), Builder->getDoubleTy()), calltmp1);
    Builder->CreateRet(mul);
    
    Builder->restoreIP(PrevInsertPoint);
    return F;
}

llvm::Value *BinaryOperatorAST::codegen() {
    if (Op == "=" || Op == ":=") {
        if (const auto *Var = dynamic_cast<VariableAST*>(LHS.get())) {
            return handleAssignment(RHS.get(), RHS->getType(), Var->getType(), Var->getName(), Var->getConstness(), Var->getDecl(), Var->hasComptimeVal(), Op == ":=");
        } else if (auto *Arr = dynamic_cast<AccessElementOperatorAST*>(LHS.get())) {
            llvm::Value *LHSVal = Arr->requireLValue();
            return Builder->CreateStore(RHS->codegen(), LHSVal);
        } else if (auto *Deref = dynamic_cast<DereferenceOperatorAST*>(LHS.get())) {
            llvm::Value *LHSVal = Deref->requireLValue();
            StoreOrMemCpy(RHS.get(), RHS->getType(), LHSVal, Deref->getType());
            return nullptr;
        } else {
            babel_panic("Destination of '=' must be assignable");
        }
    }

    llvm::Value *left = LHS->codegen();
    llvm::Value *right = RHS->codegen();
    if (!left || !right) return nullptr;

    BabelType lTy = LHS->getType();
    BabelType rTy = RHS->getType();

    using enum OpKind;
    switch (getOperation(Op, lTy, rTy)) {
        case Div: {
            if (isBabelInteger(lTy) && isBabelInteger(rTy)) {
                llvm::Type* doubleTy = llvm::Type::getDoubleTy(*TheContext);
                left = Builder->CreateSIToFP(left, doubleTy, "lhsfp");
                right = Builder->CreateSIToFP(right, doubleTy, "rhsfp");
            } else {
                if (canImplicitCast(lTy, rTy)) {
                    left = performImplicitCast(left, lTy, rTy);
                } else if (canImplicitCast(rTy, lTy)) {
                    right = performImplicitCast(right, rTy, lTy);
                } else {
                    babel_panic("Types dont match for binary operator; implicit cast failed or is not allowed");
                }
            }
            break;
        }
        case PowerFloatInt: {
            if (lTy == BabelType::Float16()) {
                llvm::Type* floatTy = llvm::Type::getFloatTy(*TheContext);
                left = Builder->CreateFPExt(left, floatTy, "lhsf32");
            }
            right = Builder->CreateSExtOrTrunc(right, llvm::Type::getInt32Ty(*TheContext));
            break;
        }
        case PowerFloat: {
            if ((lTy == BabelType::Float16() || isBabelInteger(lTy)) && rTy == BabelType::Float16()) {
                llvm::Type* floatTy = llvm::Type::getFloatTy(*TheContext);
                left = isBabelInteger(lTy) ? Builder->CreateSIToFP(left, floatTy, "lhsf32") : Builder->CreateFPExt(left, floatTy, "lhsf32");
                right = Builder->CreateFPExt(right, floatTy, "rhsf32");
                lTy = BabelType::Float32();
                rTy = BabelType::Float32();
                break;
            }
            /* fallthrough */
        }
        case AddInt: case AddFloat: case SubInt: case SubFloat:
        case MulInt: case MulFloat: case IDiv: case RemInt:
        case RemFloat: case Shl: case Shr: case LShr:
        case PowerInt: case BitAnd: case BitXor: case BitOr: {
            if (canImplicitCast(lTy, rTy)) {
                left = performImplicitCast(left, lTy, rTy);
            } else if (canImplicitCast(rTy, lTy)) {
                right = performImplicitCast(right, rTy, lTy);
            } else {
                babel_panic("Types dont match for binary operator; implicit cast failed or is not allowed");
            }
        }
        default:
            break;
    }

    switch (getOperation(Op, lTy, rTy)) {
        case AddInt:
            return Builder->CreateAdd(left, right, "addtmp");
        case AddFloat:
            return Builder->CreateFAdd(left, right, "addtmp");
        case AddPtr:
            return Builder->CreateInBoundsGEP(
                resolveLLVMType(lTy.isPointer() ? *lTy.getPointer().to : *rTy.getPointer().to),
                lTy.isPointer() ? left : right,
                {!lTy.isPointer() ? left : right},
                "paddtmp"
            );
        case SubInt:
            return Builder->CreateSub(left, right, "subtmp");
        case SubFloat:
            return Builder->CreateFSub(left, right, "subtmp");
        case SubPtr:
            return Builder->CreateInBoundsGEP(
                resolveLLVMType(lTy.isPointer() ? *lTy.getPointer().to : *rTy.getPointer().to),
                lTy.isPointer() ? left : right,
                {Builder->CreateNeg(!lTy.isPointer() ? left : right)},
                "psubtmp"
            );
        case PtrDiff:
            return Builder->CreatePtrDiff(resolveLLVMType(*lTy.getPointer().to), left, right, "ptrdiff");
        case MulInt:
            return Builder->CreateMul(left, right, "multmp");
        case MulFloat:
            return Builder->CreateFMul(left, right, "multmp");
        case MulBool: {
            llvm::Value* b = lTy == BabelType::Boolean() ? left : right;
            llvm::Value* num = lTy == BabelType::Boolean() ? right : left;
            llvm::Function* copysign = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::copysign, {num->getType(), num->getType()});

            llvm::Value* zeroVal = num->getType()->isFloatingPointTy()
                ? Builder->CreateCall(copysign, {llvm::ConstantFP::getZero(num->getType()), num})
                : llvm::cast<llvm::Value>(llvm::ConstantInt::get(num->getType(), 0));
            return Builder->CreateSelect(b, num, zeroVal, "multmp");
        }
        case Div:
            return Builder->CreateFDiv(left, right, "divtmp");
        case IDiv:
            return Builder->CreateSDiv(left, right, "idivtmp");
        case RemInt:
            return Builder->CreateSRem(left, right, "remtmp");
        case RemFloat:
            return Builder->CreateFRem(left, right, "remtmp");
        case PowerInt: {
            assert(left->getType() == right->getType());
            llvm::Function* F = getOrCreate_ipow(left->getType());
            return Builder->CreateCall(F, {left, right}, "powtmp");
        }
        case PowerFloatInt: {
            llvm::Function* powi = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::powi, {left->getType(), right->getType()});
            llvm::Value* call = Builder->CreateCall(powi, {left, right}, "powtmp");
            return lTy == BabelType::Float16() ? Builder->CreateFPTrunc(call, resolveLLVMType(lTy)) : call;
        }
        case PowerFloat: {
            llvm::Function* pow = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::pow, {left->getType(), right->getType()});
            llvm::Value* call = Builder->CreateCall(pow, {left, right}, "powtmp");
            return lTy == BabelType::Float16() && rTy == BabelType::Float16() ? Builder->CreateFPTrunc(call, resolveLLVMType(lTy)) : call;
        }
        case Shl:
            return Builder->CreateShl(left, right, "shltmp");
        case Shr:
            return Builder->CreateAShr(left, right, "shrtmp");
        case LShr:
            return Builder->CreateLShr(left, right, "lshrtmp");
        case BitAnd:
            return Builder->CreateAnd(left, right, "andtmp");
        case BitXor:
            return Builder->CreateXor(left, right, "xortmp");
        case BitOr:
            return Builder->CreateOr(left, right, "ortmp");
        
        default:
            babel_panic("Invalid binary operator %s(%s, %s)", Op.c_str(), getBabelTypeName(lTy).c_str(), getBabelTypeName(rTy).c_str());
    }
}

llvm::Value *UnaryOperatorAST::codegen() {
    llvm::Value *operand = Val->codegen();
    BabelType ty = Val->getType();
    if (!operand) return nullptr;

    using enum OpKind;
    switch (getOperation(Op, ty)) {
        case Not:
            return Builder->CreateNot(operand, "nottmp");
        case Neg:
            return Builder->CreateNeg(operand, "negtmp");
        case FNeg:
            return Builder->CreateFNeg(operand, "negtmp");
        case Id:
            return operand; // unary plus is the identity operation (i.e. a no-op)
        case PreInc: {
            llvm::Value* inc = Builder->CreateAdd(operand, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1), "inc");
            Builder->CreateStore(inc, Val->requireLValue());
            return inc;
        }
        case PreDec: {
            llvm::Value* dec = Builder->CreateSub(operand, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1), "dec");
            Builder->CreateStore(dec, Val->requireLValue());
            return dec;
        }
        case PostInc: {
            llvm::Value* inc = Builder->CreateAdd(operand, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1), "inc");
            Builder->CreateStore(inc, Val->requireLValue());
            return operand;
        }
        case PostDec: {
            llvm::Value* dec = Builder->CreateSub(operand, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1), "dec");
            Builder->CreateStore(dec, Val->requireLValue());
            return operand;
        }
        case PrePtrInc: {
            llvm::Value* inc = Builder->CreateInBoundsGEP(resolveLLVMType(ty), operand, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1)}, "ptrinc");
            Builder->CreateStore(inc, Val->requireLValue());
            return inc;
        }
        case PrePtrDec: {
            llvm::Value* dec = Builder->CreateInBoundsGEP(resolveLLVMType(ty), operand, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), -1)}, "ptrdec");
            Builder->CreateStore(dec, Val->requireLValue());
            return dec;
        }
        case PostPtrInc: {
            llvm::Value* inc = Builder->CreateInBoundsGEP(resolveLLVMType(ty), operand, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1)}, "ptrinc");
            Builder->CreateStore(inc, Val->requireLValue());
            return operand;
        }
        case PostPtrDec: {
            llvm::Value* dec = Builder->CreateInBoundsGEP(resolveLLVMType(ty), operand, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), -1)}, "ptrdec");
            Builder->CreateStore(dec, Val->requireLValue());
            return operand;
        }

        default:
            babel_panic("Invalid unary operator %s(%s)", Op.c_str(), getBabelTypeName(ty).c_str());
    }
}

llvm::Value *AccessElementOperatorAST::codegen() {
    if (!isBabelInteger(Index->getType()))
        babel_panic("Element access must use integer index");
    
    if (!Container->getType().isArray())
        babel_panic("'%s' object is not subscriptable", getBabelTypeName(Container->getType()).c_str());

    // maybe bounds check, or opt for a C++ like setup with at and operator[]
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
    llvm::Value* elmntPtr = Builder->CreateInBoundsGEP(resolveLLVMType(Container->getType()), Container->requireLValue(), {zero, Index->codegen()}, "elmntPtr");

    if (requiresLValue) {
        if (auto const* var = dynamic_cast<VariableAST*>(Container.get()); var && var->getConstness())
            babel_panic("The underlying array is constant");

        return elmntPtr;
    }
    
    return Builder->CreateLoad(resolveLLVMType(*(Container->getType().getArray().inner)), elmntPtr, "arrtmp");
}

llvm::Value *DereferenceOperatorAST::codegen() {
    if (!Var->getType().isPointer()) {
        babel_panic("Cannot dereference non-pointer");
    }

    if (requiresLValue) {
        if (Var->getType().getPointer().pointsToConst)
            babel_panic("The pointer points to constant data");

        return Var->codegen();
    }

    return Builder->CreateLoad(resolveLLVMType(*(Var->getType().getPointer().to)), Var->codegen(), "dereftmp");
}

llvm::Value *AddressOfOperatorAST::codegen() {
    return Var->requireLValue();
}

llvm::Value *ContinueStmtAST::codegen() {
    if (Target.has_value()) {
        if (!LoopTable.contains(Target.value()))
            babel_panic("undefined loop label");

        Builder->CreateBr(LoopTable.at(Target.value()).__continue__);
    } else {
        if (LoopTable.at(".active").__continue__ == nullptr)
            babel_panic("continue statement outside of loop not allowed");
        
        Builder->CreateBr(LoopTable.at(".active").__continue__);
    }

    return nullptr;
}

llvm::Value *BreakStmtAST::codegen() {
    if (Target.has_value()) {
        if (!LoopTable.contains(Target.value()))
            babel_panic("undefined loop label");

        Builder->CreateBr(LoopTable.at(Target.value()).__break__);
    } else {
        if (LoopTable.at(".active").__break__ == nullptr)
            babel_panic("break statement outside of loop not allowed");
        
        Builder->CreateBr(LoopTable.at(".active").__break__);
    }

    return nullptr;
}

llvm::Value *ReturnStmtAST::codegen() {
    const llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    if (GLOBAL_SCOPE)
        babel_panic("Return statements must be inside of a task");

    if (Expr) {
        llvm::Value *RetVal = Expr->codegen();
        if (canImplicitCast(Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret)) {
            RetVal = performImplicitCast(RetVal, Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret);
            Builder->CreateRet(RetVal);
        } else {
            babel_panic("Task return type does not match returned value (returned %s but expected %s); implicit cast failed or is not allowed", 
                getBabelTypeName(Expr->getType()).c_str(), getBabelTypeName(TaskTable.at(TheFunction->getName().str()).ret).c_str());
        }
    } else {
        Builder->CreateRetVoid();
    }

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

llvm::Value *WhileLoopAST::codegen() {
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*TheContext, "for.cond", TheFunction);
    llvm::BasicBlock *BodyBB = llvm::BasicBlock::Create(*TheContext, "for.body");
    llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(*TheContext, "for.end");

    LoopInfo previous = LoopTable.at(".active");
    LoopTable[".active"] = {CondBB, EndBB};

    if (Label.has_value()) {
        if (LoopTable.contains(Label.value()))
            babel_panic("loop label already exists in outer loop");

        LoopTable[Label.value()] = {CondBB, EndBB};
    }

    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    llvm::Value* CondV = Cond->codegen();

    if (!CondV->getType()->isIntegerTy(1))
        babel_panic("loop condition doesn't meet requirement: Boolean Type");

    Builder->CreateCondBr(CondV, BodyBB, EndBB);

    TheFunction->insert(TheFunction->end(), BodyBB);
    Builder->SetInsertPoint(BodyBB);
    Body->codegen();
    Builder->CreateBr(CondBB);

    TheFunction->insert(TheFunction->end(), EndBB);
    Builder->SetInsertPoint(EndBB);

    LoopTable[".active"] = previous;
    if (Label.has_value())
        LoopTable.erase(Label.value());

    return nullptr;
}

llvm::Value *ForLoopAST::codegen() {
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*TheContext, "for.cond", TheFunction);
    llvm::BasicBlock *BodyBB = llvm::BasicBlock::Create(*TheContext, "for.body");
    llvm::BasicBlock *UpdateBB = llvm::BasicBlock::Create(*TheContext, "for.inc");
    llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(*TheContext, "for.end");

    LoopInfo previous = LoopTable.at(".active");
    LoopTable[".active"] = {UpdateBB, EndBB};

    if (Label.has_value()) {
        if (LoopTable.contains(Label.value()))
            babel_panic("loop label already exists in outer loop");

        LoopTable[Label.value()] = {UpdateBB, EndBB};
    }

    Init->codegen();
    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    llvm::Value* CondV = Cond->codegen();

    if (!CondV->getType()->isIntegerTy(1))
        babel_panic("loop condition doesn't meet requirement: Boolean Type");

    Builder->CreateCondBr(CondV, BodyBB, EndBB);

    TheFunction->insert(TheFunction->end(), BodyBB);
    Builder->SetInsertPoint(BodyBB);
    Body->codegen();
    Builder->CreateBr(UpdateBB);

    TheFunction->insert(TheFunction->end(), UpdateBB);
    Builder->SetInsertPoint(UpdateBB);
    Update->codegen();
    Builder->CreateBr(CondBB);

    TheFunction->insert(TheFunction->end(), EndBB);
    Builder->SetInsertPoint(EndBB);

    LoopTable[".active"] = previous;
    if (Label.has_value())
        LoopTable.erase(Label.value());

    return nullptr;
}

llvm::Value *ForInLoopAST::codegen() {
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(*TheContext, "for.cond", TheFunction);
    llvm::BasicBlock *BodyBB = llvm::BasicBlock::Create(*TheContext, "for.body");
    llvm::BasicBlock *UpdateBB = llvm::BasicBlock::Create(*TheContext, "for.inc");
    llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(*TheContext, "for.end");

    LoopInfo previous = LoopTable.at(".active");
    LoopTable[".active"] = {UpdateBB, EndBB};

    if (Label.has_value()) {
        if (LoopTable.contains(Label.value()))
            babel_panic("loop label already exists in outer loop");

        LoopTable[Label.value()] = {UpdateBB, EndBB};
    }

    if (!Collection->getType().isArray())
        babel_panic("for in loop must use iterable collection");
    
    // alternatively an iterator type
    llvm::AllocaInst* it = Builder->CreateAlloca(llvm::PointerType::getUnqual(*TheContext), nullptr, "it");
    llvm::AllocaInst* end = Builder->CreateAlloca(llvm::PointerType::getUnqual(*TheContext), nullptr, "end");

    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
    llvm::Value* length = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), Collection->getType().getArray().size);

    // alternatively call Iterable.begin()
    llvm::Value* front = Builder->CreateInBoundsGEP(resolveLLVMType(Collection->getType()), Collection->requireLValue(), {zero, zero});
    Builder->CreateStore(front, it);

    // alternatively call Iterable.end()
    llvm::Value* back = Builder->CreateInBoundsGEP(resolveLLVMType(Collection->getType()), Collection->requireLValue(), {zero, length});
    Builder->CreateStore(back, end);

    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    // alternatively Iterator.__operator_compare(it, end) != 0
    llvm::Value *comp = Builder->CreateICmpNE(Builder->CreateLoad(llvm::PointerType::getUnqual(*TheContext), it), Builder->CreateLoad(llvm::PointerType::getUnqual(*TheContext), end), "cmp");

    Builder->CreateCondBr(comp, BodyBB, EndBB);

    TheFunction->insert(TheFunction->end(), BodyBB);
    Builder->SetInsertPoint(BodyBB);

    const auto *Var = dynamic_cast<VariableAST*>(Elmnt.get());
    llvm::AllocaInst* a = Builder->CreateAlloca(resolveLLVMType(*Collection->getType().getArray().inner), nullptr, Var->getName());
    NamedValues[Var->getName()] = {a, *Collection->getType().getArray().inner, false};
    // manual dereference, alternatively call Iterator.current()
    Builder->CreateStore(Builder->CreateLoad(resolveLLVMType(*Collection->getType().getArray().inner), Builder->CreateLoad(llvm::PointerType::getUnqual(*TheContext), it)), a);
    Body->codegen();
    
    Builder->CreateBr(UpdateBB);

    TheFunction->insert(TheFunction->end(), UpdateBB);
    Builder->SetInsertPoint(UpdateBB);

    // alternatively call Iterator.advance()
    llvm::Value* it1 = Builder->CreateLoad(llvm::PointerType::getUnqual(*TheContext), it);
    llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 1);
    llvm::Value* incDec = Builder->CreateInBoundsGEP(llvm::Type::getInt32Ty(*TheContext), it1, {one}, "incdec");
    Builder->CreateStore(incDec, it);
    
    Builder->CreateBr(CondBB);

    TheFunction->insert(TheFunction->end(), EndBB);
    Builder->SetInsertPoint(EndBB);

    LoopTable[".active"] = previous;
    if (Label.has_value())
        LoopTable.erase(Label.value());

    return nullptr;
}

BabelType MacroCallAST::getType() const {
    if (name == "va_arg") {
        if (Args.size() != 2 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]) || !std::holds_alternative<BabelType>(Args[1]))
            babel_panic("@va_arg requires list name and type parameter");
        
        return std::get<BabelType>(Args[1]);
    } else {
        return BabelType::Void();
    }
}

llvm::Value *MacroCallAST::codegen() {
    if (name == "va_list") {

        #if defined(_M_ARM64) || defined(__aarch64__)
            #define __BUILTIN_VA_LIST llvm::StructType::get(*TheContext, {\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::IntegerType::getInt32Ty(*TheContext),\
                llvm::IntegerType::getInt32Ty(*TheContext)\
            })
        #elif defined(_WIN32) || defined(__i386__)
            #define __BUILTIN_VA_LIST llvm::PointerType::get(*TheContext, 0)
        #elif defined(__powerpc__)
            #define __BUILTIN_VA_LIST llvm::ArrayType::get(llvm::StructType::get(*TheContext, {\
                llvm::IntegerType::getInt8Ty(*TheContext),\
                llvm::IntegerType::getInt8Ty(*TheContext),\
                llvm::IntegerType::getInt16Ty(*TheContext),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0)\
            }), 1)
        #elif defined(__s390__) || defined(__zarch__)
            #define __BUILTIN_VA_LIST llvm::ArrayType::get(llvm::StructType::get(*TheContext, {\
                llvm::IntegerType::getInt64Ty(*TheContext),\
                llvm::IntegerType::getInt64Ty(*TheContext),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0)\
            }), 1)
        #elif defined(__hexagon__)
            #define __BUILTIN_VA_LIST llvm::StructType::get(*TheContext, {\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0)\
            })
        #elif defined(__xtensa__)
            #define __BUILTIN_VA_LIST llvm::StructType::get(*TheContext, {\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::IntegerType::getInt32Ty(*TheContext)\
            })
        #elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
            #define __BUILTIN_VA_LIST llvm::ArrayType::get(llvm::StructType::get(*TheContext, {\
                llvm::IntegerType::getInt32Ty(*TheContext),\
                llvm::IntegerType::getInt32Ty(*TheContext),\
                llvm::PointerType::get(*TheContext, 0),\
                llvm::PointerType::get(*TheContext, 0)\
            }), 1)
        #else
            // most other platforms like ARM32 just use a { ptr }, so thats our best guess
            #define __BUILTIN_VA_LIST llvm::StructType::get(*TheContext, llvm::ArrayRef<llvm::Type*>{llvm::PointerType::get(*TheContext, 0)})
        #endif

        llvm::AllocaInst* ap = Builder->CreateAlloca(__BUILTIN_VA_LIST);

        if (Args.size() != 1 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]))
            babel_panic("@va_list requires name parameter");

        auto var = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[0]).get());
        NamedValues[var->getName()] = {ap, BabelType::Pointer(TheArena.make(BabelType::Void()), true), true};

        return nullptr;
    } else if (name == "va_start") {
        llvm::Function* va_start = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::vastart, {llvm::PointerType::get(*TheContext, 0)});
        
        if (Args.size() != 1 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]))
            babel_panic("@va_start requires name parameter");

        auto var = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[0]).get());
        llvm::Value* ap = NamedValues.at(var->getName()).val;

        if (llvm::Type* type = ap->getType(); type->isArrayTy()) {
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
            ap = Builder->CreateInBoundsGEP(type, ap, {zero, zero});
        }

        Builder->CreateCall(va_start, {ap});
        return nullptr;
    } else if (name == "va_end") {
        llvm::Function* va_end = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::vaend, {llvm::PointerType::get(*TheContext, 0)});
        
        if (Args.size() != 1 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]))
            babel_panic("@va_end requires name parameter");

        auto var = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[0]).get());
        llvm::Value* ap = NamedValues.at(var->getName()).val;

        if (llvm::Type* type = ap->getType(); type->isArrayTy()) {
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
            ap = Builder->CreateInBoundsGEP(type, ap, {zero, zero});
        }

        Builder->CreateCall(va_end, {ap});
        return nullptr;
    } else if (name == "va_copy") {
        llvm::Function* va_copy = llvm::Intrinsic::getDeclaration(TheModule.get(), llvm::Intrinsic::vacopy, {llvm::PointerType::get(*TheContext, 0), llvm::PointerType::get(*TheContext, 0)});
        
        if (Args.size() != 2 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]) || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[1]))
            babel_panic("@va_copy requires destination and source parameter");

        auto dst = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[0]).get());
        auto src = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[1]).get());
        llvm::Value* ap = NamedValues.at(dst->getName()).val;
        llvm::Value* aq = NamedValues.at(src->getName()).val;

        if (llvm::Type* type = ap->getType(); type->isArrayTy()) {
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
            ap = Builder->CreateInBoundsGEP(type, ap, {zero, zero});
            aq = Builder->CreateInBoundsGEP(type, aq, {zero, zero});
        }

        Builder->CreateCall(va_copy, {ap, aq});
        return nullptr;
    } else if (name == "va_arg") {
        if (Args.size() != 2 || !std::holds_alternative<std::unique_ptr<BaseAST>>(Args[0]) || !std::holds_alternative<BabelType>(Args[1]))
            babel_panic("@va_arg requires list name and type parameter");

        auto var = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(Args[0]).get());
        llvm::Value* ap = NamedValues.at(var->getName()).val;

        if (llvm::Type* type = ap->getType(); type->isArrayTy()) {
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext), 0);
            ap = Builder->CreateInBoundsGEP(type, ap, {zero, zero});
        }
        
        return Builder->CreateVAArg(ap, resolveLLVMType(std::get<BabelType>(Args[1])));
    } else {
        babel_panic("No macro with name @%s exists", name.c_str());
    }
}

llvm::Value *TaskCallAST::codegen() {
    if (callsTo == "main") babel_panic("Calling main is not allowed, as the programs entry point it is invoked automatically");

    if (PolymorphTable.at(callsTo)) {
        auto underscore_fold = [](std::string a, const std::unique_ptr<BaseAST>& b) { return std::move(a) + '_' + getBabelTypeName(b->getType()); };
 
        std::string typeinfo = !Args.empty() ? std::accumulate(std::next(Args.begin()), Args.end(), getBabelTypeName(Args[0]->getType()), underscore_fold) : "";
        std::string name = std::format("{}.polymorphic.{}", callsTo, typeinfo);

        if (!TaskTable.contains(name)) {
            auto matches = [&](const std::pair<std::string, TaskTypeInfo>& kv) {
                auto argTypes = Args | std::views::transform([](const std::unique_ptr<BaseAST>& elmnt){ return elmnt->getType(); });

                return kv.first.starts_with(callsTo) 
                    && kv.first.ends_with("...")
                    && std::ranges::equal(kv.second.args, argTypes | std::views::take(kv.second.args.size()));
            };

            auto filtered = TaskTable | std::views::filter(matches);
            auto size = std::distance(filtered.begin(), filtered.end());

            if (size == 1) {
                name = (*filtered.begin()).first;
            } else if (size > 1) {
                std::string candidates = "";
                for (const auto&[_, value] : filtered) {
                    candidates += formatArgs(value.args, value.isVarArg) + '\n';
                }
                babel_panic("ambiguous call of polymorphic task '%s', multiple matching tasks exist:\n%s", callsTo.c_str(), candidates.c_str());
            } else {
                std::string expected = "";
                for (const auto&[key, value] : TaskTable) {
                    if (key.starts_with(callsTo + ".polymorphic")) {
                        expected += formatArgs(value.args, value.isVarArg) + '\n';
                    }
                }
                babel_panic("No matching overload for polymorphic task '%s', only the following were valid:\n%s", callsTo.c_str(), expected.c_str());
            }
        }

        callsTo = name;
    }

    llvm::Function *CalleF = TheModule->getFunction(callsTo);
    if (!CalleF) babel_panic("Unknown Task '%s' referenced", callsTo.c_str());

    if (CalleF->arg_size() != Args.size() && !CalleF->isVarArg())
        babel_panic("Passed incorrect number of arguments (expected %d but got %d)", static_cast<int>(CalleF->arg_size()), static_cast<int>(Args.size()));

    if (CalleF->arg_size() > Args.size() && CalleF->isVarArg())
        babel_panic("vararg task needs at least %d arguments but got only %d", static_cast<int>(CalleF->arg_size()), static_cast<int>(Args.size()));

    std::vector<llvm::Value *> ArgsV;
    for (unsigned int i = 0, e = Args.size(); i != e; ++i) {
        llvm::Value *val = Args[i]->codegen();
        if (canImplicitCast(Args[i]->getType(), TaskTable.at(callsTo).args[i]))
            val = performImplicitCast(val, Args[i]->getType(), TaskTable.at(callsTo).args[i]);
        
        ArgsV.push_back(val);
    }

    if (TaskTable.at(callsTo).ret == BabelType::Void())
        return Builder->CreateCall(CalleF, ArgsV);
    return Builder->CreateCall(CalleF, ArgsV, "calltmp");
}

llvm::Function *TaskHeaderAST::codegen() {
    update();
    std::vector<llvm::Type*> Types;
    //std::ranges::transform(ArgTypes, Types.begin(), [](const BabelType& type) { return resolveLLVMType(type); });
    for (const BabelType& type : ArgTypes) {
        Types.push_back(resolveLLVMType(type));
    }

    llvm::FunctionType *FT = llvm::FunctionType::get(resolveLLVMType(ReturnType), Types, isVarArg);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get());

    unsigned int idx = 0;
    for (auto &Arg : F->args()) {
        Arg.setName(Args[idx++]);
    }

    return F;
}

llvm::Function *TaskAST::codegen() {
    Header->update();
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
        if (Header->getRetType() == BabelType::Void())
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

    GlobalValues["__argc__"] = {GArgc, BabelType::Int32(), false, false};
    GlobalValues["__argv__"] = {GArgv, BabelType::CString(), false, false};
    GlobalValues["__envp__"] = {GEnvp, BabelType::CString(), false, false};

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