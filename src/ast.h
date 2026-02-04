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
};

// static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
// static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, LocalSymbol> NamedValues;
static std::map<std::string, GlobalSymbol> GlobalValues;
static std::map<std::string, llvm::BasicBlock*> LabelTable;
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
            if (isDecl) { insertSymbol(); }
            else { this->isConst = GlobalValues.at(Name).isConstant; this->isComptime = GlobalValues.at(Name).isComptime; }
            // TODO: each variable auto updates itself so the node knows wether its variable is constant or not
            // this allows for a significant simplification of the handleAssignment method
        }
        void insertSymbol() const;
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
    std::string callsTo;
    std::deque<std::unique_ptr<BaseAST>> Args;

    public:
        TaskCallAST(const std::string &callsTo, std::deque<std::unique_ptr<BaseAST>> Args) : callsTo(callsTo), Args(std::move(Args)) {}
        BabelType getType() const override;
        bool isComptimeAssignable() const override { return false; } /* true if it's comptime, but that doesn't exist yet */
        llvm::Value *codegen() override;
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    std::string Name;
    std::deque<std::string> Args;
    std::deque<BabelType> ArgTypes;
    BabelType ReturnType;

    public:
        TaskHeaderAST(const std::string &Name, std::deque<std::string> Args, std::deque<BabelType> ArgTypes, BabelType ReturnType) : Name(Name), Args(std::move(Args)), ArgTypes(std::move(ArgTypes)), ReturnType(ReturnType) {
            TaskTable[Name] = {this->ArgTypes, ReturnType};
            PolymorphTable[Name] = PolymorphTable.contains(Name);
        }
        llvm::Function *codegen() override;
        const std::string &getName() const { return Name; }
        const std::deque<BabelType> &getArgTypes() const { return ArgTypes; }
        const BabelType &getRetType() const { return ReturnType; }
        void update() {
            if (PolymorphTable.contains(Name) && PolymorphTable.at(Name)) {
                auto underscore_fold = [](std::string a, BabelType b) { return std::move(a) + '_' + getBabelTypeName(b); };
 
                std::string typeinfo = std::accumulate(std::next(ArgTypes.begin()), ArgTypes.end(), getBabelTypeName(ArgTypes[0]), underscore_fold);

                auto node_handle = TaskTable.extract(Name);
                if (!node_handle.empty()) {
                    node_handle.key() = std::format("{}.polymorphic.{}", Name, typeinfo);
                    node_handle.mapped() = {ArgTypes, ReturnType};
                    TaskTable.insert(std::move(node_handle));
                } else {
                    TaskTable[std::format("{}.polymorphic.{}", Name, typeinfo)] = {ArgTypes, ReturnType};
                }
                Name = std::format("{}.polymorphic.{}", Name, typeinfo);
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
        printf("Potentially dangerous\n");
    } else {
        llvm::Value* srcVal = src->codegen();
        if (canImplicitCast(srcType, destType))
                srcVal = performImplicitCast(srcVal, srcType, destType);

        Builder->CreateStore(srcVal, dest);
    }
}

llvm::Value *handleAssignment(BaseAST* RHS, BabelType RHSType, BabelType VarType, const std::string& VarName, const bool isConst, const bool isDeclaration, const bool isComptime) {
    if (GLOBAL_SCOPE) {
        // we are in global scope

        if (GlobalValues.contains(VarName) && GlobalValues.at(VarName).val != nullptr) {
            if (isDeclaration) {
                babel_panic("Redefinition of global variable '%s'", VarName.c_str());
            }

            GlobalSymbol& existing = GlobalValues[VarName];
            if (existing.isConstant) {
                babel_panic("Cannot assign to constant '%s'", VarName.c_str());
            }

            StoreOrMemCpy(RHS, RHSType, existing.val, existing.type);
            return existing.val;
        }

        if (!isDeclaration) {
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
                if (isDeclaration) {
                    babel_panic("Redefinition of global variable '%s'", VarName.c_str());
                }
    
                GlobalSymbol& existing = GlobalValues[VarName];
                if (existing.isConstant) {
                    babel_panic("Cannot assign to constant '%s'", VarName.c_str());
                }

                StoreOrMemCpy(RHS, RHSType, existing.val, existing.type);
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

llvm::Value *BinaryOperatorAST::codegen() {
    if (Op == "=") {        
        if (const auto *Var = dynamic_cast<VariableAST*>(LHS.get())) {
            return handleAssignment(RHS.get(), RHS->getType(), Var->getType(), Var->getName(), Var->getConstness(), Var->getDecl(), Var->hasComptimeVal());
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
        babel_panic("Invalid binary operator %s", Op.c_str());
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

llvm::Value *ReturnStmtAST::codegen() {
    llvm::Function const *TheFunction = Builder->GetInsertBlock()->getParent();
    if (GLOBAL_SCOPE)
        babel_panic("Return statements must be inside of a task");

    if (Expr) {
        llvm::Value *RetVal = Expr->codegen();
        if (canImplicitCast(Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret))
            RetVal = performImplicitCast(RetVal, Expr->getType(), TaskTable.at(TheFunction->getName().str()).ret);
        Builder->CreateRet(RetVal);
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

llvm::Value *TaskCallAST::codegen() {
    if (callsTo == "main") babel_panic("Calling main is not allowed, as the programs entry point it is invoked automatically");

    if (PolymorphTable.at(callsTo)) {
        auto underscore_fold = [](std::string a, const std::unique_ptr<BaseAST>& b) { return std::move(a) + '_' + getBabelTypeName(b->getType()); };
 
        std::string typeinfo = std::accumulate(std::next(Args.begin()), Args.end(), getBabelTypeName(Args[0]->getType()), underscore_fold);

        std::string name = std::format("{}.polymorphic.{}", callsTo, typeinfo);
        if (!TaskTable.contains(name)) {
            std::string expected = "";
            for (const auto&[key, value] : TaskTable) {
                if (key.starts_with(callsTo + ".polymorphic")) {
                    std::vector<std::string> strArgs;
                    std::ranges::transform(value.args, std::back_inserter(strArgs), [](BabelType t) { return getBabelTypeName(t); });
                    expected += std::format("({})\n", boost::algorithm::join(strArgs, ", "));
                }
            }
            babel_panic("Task '%s' was called with argument list %s but only the following were valid:\n%s", callsTo.c_str(), typeinfo.c_str(), expected.c_str());
        }

        callsTo = name;
    }

    llvm::Function *CalleF = TheModule->getFunction(callsTo);
    if (!CalleF) babel_panic("Unknown Task '%s' referenced", callsTo.c_str());

    if (CalleF->arg_size() != Args.size()) babel_panic("Passed incorrect number of arguments (expected %d but got %d)", static_cast<int>(CalleF->arg_size()), static_cast<int>(Args.size()));

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

    llvm::FunctionType *FT = llvm::FunctionType::get(resolveLLVMType(ReturnType), Types, false);
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