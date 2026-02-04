#ifndef TYPING_H
#define TYPING_H

#include <boost/functional/hash.hpp>
#include <unordered_map>
#include "llvm/IR/Type.h"
#include "util.hpp"
#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>

enum class BasicType {
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

struct BabelType;
struct ArrayType {
    const BabelType* inner;
    size_t size;

    bool operator==(const ArrayType& that) const;
};

struct PointerType {
    const BabelType* to;
    bool pointsToConst;

    bool operator==(const PointerType& that) const;
};

struct BabelType {
    std::variant<BasicType, ArrayType, PointerType> type;

    static BabelType Int() { return BabelType{BasicType::Int32}; }
    static BabelType Int8() { return BabelType{BasicType::Int8}; }
    static BabelType Int16() { return BabelType{BasicType::Int16}; }
    static BabelType Int32() { return BabelType{BasicType::Int32}; }
    static BabelType Int64() { return BabelType{BasicType::Int64}; }
    static BabelType Int128() { return BabelType{BasicType::Int128}; }
    static BabelType IntN(uint bitWidth);
    static BabelType Float() { return BabelType{BasicType::Float32}; }
    static BabelType Float16() { return BabelType{BasicType::Float16}; }
    static BabelType Float32() { return BabelType{BasicType::Float32}; }
    static BabelType Float64() { return BabelType{BasicType::Float64}; }
    static BabelType Float128() { return BabelType{BasicType::Float128}; }
    static BabelType Boolean() { return BabelType{BasicType::Boolean}; }
    static BabelType Character() { return BabelType{BasicType::Character}; }
    static BabelType CString() { return BabelType{BasicType::CString}; }
    static BabelType Void() { return BabelType{BasicType::Void}; }
    static BabelType Array(const BabelType* inner, size_t size) { return BabelType{ArrayType{inner, size}}; }
    static BabelType Pointer(const BabelType* to, const bool pointsToConst) { return BabelType{PointerType{to, pointsToConst}}; }

    bool isBasic() const { return std::holds_alternative<BasicType>(type); }
    bool isArray() const { return std::holds_alternative<ArrayType>(type); }
    bool isPointer() const { return std::holds_alternative<PointerType>(type); }

    BasicType getBasic() const { return std::get<BasicType>(type); }
    ArrayType getArray() const { return std::get<ArrayType>(type); }
    PointerType getPointer() const { return std::get<PointerType>(type); }

    bool operator==(const BabelType& that) const = default;
};

BabelType BabelType::IntN(uint bitWidth) {
    switch (bitWidth) {
        case 8: return BabelType::Int8();
        case 16: return BabelType::Int16();
        case 32: return BabelType::Int32();
        case 64: return BabelType::Int64();
        case 128: return BabelType::Int128();
        default: babel_panic("unrecognized bit width");
    }
}

bool ArrayType::operator==(const ArrayType& that) const {
    return *inner == *that.inner && size == that.size;
}

bool PointerType::operator==(const PointerType& that) const {
    return *to == *that.to && pointsToConst == that.pointsToConst;
}
template <>
struct std::hash<ArrayType> {
    size_t operator()(const ArrayType& a) const {
        size_t seed = 0;
        boost::hash_combine(seed, *a.inner);
        boost::hash_combine(seed, a.size);
        return seed;
    }
};

template <>
struct std::hash<PointerType> {
    size_t operator()(const PointerType& p) const {
        size_t seed = 0;
        boost::hash_combine(seed, *p.to);
        boost::hash_combine(seed, p.pointsToConst);
        return seed;
    }
};
template <>
struct std::hash<BabelType> {
    size_t operator()(const BabelType& t) const {
        return boost::hash_value(t.type);
    }
};

inline std::size_t hash_value(const ArrayType& a) {
    std::size_t seed = 0;
    boost::hash_combine(seed, *a.inner);
    boost::hash_combine(seed, a.size);
    return seed;
}

inline std::size_t hash_value(const PointerType& p) {
    std::size_t seed = 0;
    boost::hash_combine(seed, *p.to);
    boost::hash_combine(seed, p.pointsToConst);
    return seed;
}

inline std::size_t hash_value(const BabelType& t) {
    return boost::hash_value(t.type); // variant hash
}

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

class TypeArena {
    std::vector<std::unique_ptr<BabelType>> storage;

public:
    template<typename... Args>
    const BabelType* make(Args&&... args) {
        storage.push_back(
            std::make_unique<BabelType>(std::forward<Args>(args)...)
        );
        return storage.back().get();
    }
};

static TypeArena TheArena;

llvm::Type *resolveLLVMType(BabelType type) {
    using enum BasicType;

    if (type.isBasic()) {
        switch (type.getBasic()) {
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
            
            case Character:
                return llvm::Type::getInt8Ty(*TheContext);

            case Void:
                return llvm::Type::getVoidTy(*TheContext);

            default:
                babel_panic("Unknow type");
        }
    } else if (type.isArray()) {
        return llvm::ArrayType::get(resolveLLVMType(*type.getArray().inner), type.getArray().size);
    }

    // return llvm::PointerType::getUnqual(resolveLLVMType(*type.getPointer().to));
    return llvm::PointerType::getUnqual(*TheContext);
}

std::string getBabelTypeName(BabelType type) {
    using enum BasicType;

    if (type.isBasic()) {
        switch (type.getBasic()) {
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
    } else if (type.isArray()) {
        return "Array<" + getBabelTypeName(*type.getArray().inner) + ">";
    } else if (type.isPointer()) {
        return getBabelTypeName(*type.getPointer().to) + "*";
    }

    babel_unreachable();
}

bool isBabelInteger(BabelType type) {
    using enum BasicType;
    if (!type.isBasic())
        return false;

    switch (type.getBasic()) {
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
    using enum BasicType;
    if (!type.isBasic())
        return false;

    switch (type.getBasic()) {
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
        {BabelType::Int8(), {BabelType::Int16(), BabelType::Int32(), BabelType::Int64(), BabelType::Int128(), BabelType::Float16(), BabelType::Float32(), BabelType::Float64(), BabelType::Float128()}},
        {BabelType::Int16(), {BabelType::Int32(), BabelType::Int64(), BabelType::Int128(), BabelType::Float16(), BabelType::Float32(), BabelType::Float64(), BabelType::Float128()}},
        {BabelType::Int32(), {BabelType::Int64(), BabelType::Int128(), BabelType::Float32(), BabelType::Float64(), BabelType::Float128()}},
        {BabelType::Int64(), {BabelType::Int128(), BabelType::Float64(), BabelType::Float128()}},
        {BabelType::Int128(), {BabelType::Float128()}},
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

#endif /* TYPING_H */
