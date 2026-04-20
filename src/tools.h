#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>

#include "llvm/ADT/APFloat.h"

#include "util.hpp"
#include "typing.h"

template <typename T>
struct is_pair : std::false_type {};

template <typename K, typename V>
struct is_pair<std::pair<K, V>> : std::true_type {};

template <typename C>
concept Container = requires {
    typename C::value_type;
    typename C::reference;
    typename C::const_reference;
    typename C::iterator;
    typename C::const_iterator;
    typename C::difference_type;
    typename C::size_type;
} && !std::is_convertible_v<C, std::string_view>;

template <typename C, typename T>
concept ContainerOf = Container<C> && std::is_same_v<typename C::value_type, T>;

template <Container Container>
std::ostream& operator<<(std::ostream& os, const Container& container) {
    bool first = true;
    os << "{";

    for (const auto& element : container) {
        if (!first) os << ", ";

        // Check if the element is a std::pair
        if constexpr (is_pair<std::decay_t<decltype(element)>>::value) {
            os << element.first << ": " << element.second;
        } else {
            os << element;
        }

        first = false;
    }

    os << "}";
    return os;
}

template <typename T, ContainerOf<T> Container>
int indexOf(const T& element, const Container& container) {
    if (auto it = std::ranges::find(container, element); it != container.end()) {
        return std::distance(container.begin(), it);
    }

    return -1;
}

template <Container Container>
bool includes(Container list1, Container list2) {
    for (const auto& elmnt : list1) {
        if (indexOf(elmnt, list2) < 0){
            return false;
        }
    }

    return true;
}

template <Container Container>
bool includeEachOther(Container list1, Container list2) {
    return includes(list1, list2) && includes(list2, list1);
}

// leave for clarity, replace later
template <typename K, typename T, typename Hash = std::hash<K>, typename Compare = std::equal_to<>>
std::list<T> getOrCreateArray(std::unordered_map<K, std::list<T>, Hash, Compare>& dict, const K& key) {
    return dict[key];
}

template<ContainerOf<std::string> Container>
Container trimElements(const Container& container) {
    Container result = {};

    for (const std::string& elmnt : container) {
        result.push_back(boost::trim_copy(elmnt));
    }

    return result;
}

template<ContainerOf<std::string> Container>
Container splitString(std::string_view input, std::string_view delimiter) {
    Container result;
    size_t startPos = 0;
    size_t foundPos;

    while ((foundPos = input.find(delimiter, startPos)) != std::string::npos) {
        result.push_back(std::string(input.substr(startPos, foundPos - startPos)));
        startPos = foundPos + delimiter.length();
    }

    // Add the last part of the string
    result.push_back(std::string(input.substr(startPos)));

    return result;
}

// leave for clarity, potentially replace later
template <typename T, ContainerOf<T> Container>
bool isElement(T elmnt, const Container& container) {
    return std::ranges::find(container, elmnt) != container.end();
}

template <typename T, ContainerOf<T> Container>
bool addUnique(T elmnt, Container& container) {
    if (!isElement(elmnt, container)) {
        container.push_back(elmnt);

        return true;
    }

    return false;
}

template <Container Container>
std::vector<typename Container::value_type> slice(const Container& in, int start = 0, std::optional<int> end = std::nullopt) {
    // Check for valid start and end indices
    if (start < 0) {
        start += in.size();
    }

    int actualEnd = (end == std::nullopt) ? in.size() : end.value();

    if (actualEnd < 0) {
        actualEnd += in.size();
    }

    if (actualEnd > in.size()) {
        actualEnd = in.size();
    }

    // Create a new list to store the sliced elements
    std::vector<typename Container::value_type> result;
    auto it = std::next(std::begin(in), start);

    while (it != std::next(std::begin(in), actualEnd)) {
        result.push_back(*it);
        ++it;
    }

    return result;
}

// zip iterator for tow ranges
// ! not able to mutate
template <std::ranges::range R1, std::ranges::range R2>
class Zipped {
public:
    Zipped(R1 r1, R2 r2) : r1_(std::move(r1)), r2_(std::move(r2)) {}

    class Iterator {
    public:
        using It1 = std::ranges::iterator_t<R1>;
        using It2 = std::ranges::iterator_t<R2>;

        Iterator(It1 it1, It2 it2) : it1_(it1), it2_(it2) {}

        bool operator!=(const Iterator& other) const {
            return it1_ != other.it1_ && it2_ != other.it2_; // stop when either hits end
        }

        void operator++() {
            ++it1_;
            ++it2_;
        }

        std::tuple<decltype(*std::declval<It1>()), decltype(*std::declval<It2>())> operator*() const {
            return std::tie(*it1_, *it2_);
        }

    private:
        It1 it1_;
        It2 it2_;
    };

    Zipped<R1, R2>::Iterator begin() {
        return Iterator{
            std::ranges::begin(r1_),
            std::ranges::begin(r2_)
        };
    }

    Zipped<R1, R2>::Iterator end() {
        return Iterator{
            std::ranges::end(r1_),
            std::ranges::end(r2_)
        };
    }

private:
    R1 r1_;
    R2 r2_;
};

std::string unescapeString(const std::string& oldstr) {
    std::ostringstream newstr;
    bool saw_backslash = false;

    for (size_t i = 0; i < oldstr.size(); ++i) {
        unsigned char ch = oldstr[i];

        if (!saw_backslash) {
            if (ch == '\\') {
                saw_backslash = true;
            } else {
                newstr << ch;
            }
            continue;
        }

        // can be removed later maybe
        if (ch == '\\') {
            saw_backslash = false;
            newstr << '\\' << '\\';
            continue;
        }

        switch (ch) {
            case 'r': newstr << '\r'; break;
            case 'n': newstr << '\n'; break;
            case 'f': newstr << '\f'; break;
            case 't': newstr << '\t'; break;
            case 'a': newstr << '\a'; break;
            case 'e': newstr << static_cast<char>(27); break; // ASCII ESC
            case 'b': newstr << "\\b"; break; // pass trough

            case 'c': {
                if (++i == oldstr.size()) babel_panic("trailing '\\c'");
                ch = oldstr[i];
                if (ch > 127) babel_panic("Expected ASCII after \\c");
                newstr << static_cast<char>(ch ^ 64);
                break;
            }

            case 'u': case 'U': {
                int len = ch == 'u' ? 4 : 8;
                if (i + len > oldstr.size()) babel_panic("string to short for unicode escape");
                std::string hex = oldstr.substr(i + 1, len);
                for (char c : hex) if (!isxdigit(c)) babel_panic("non-hex digit in Unicode escape");
                int code = std::stoi(hex, nullptr, 16);
                newstr << static_cast<char>(code); // Note: doesn't handle multi-byte unicode
                i += len;
                break;
            }

            case '8': case '9': babel_panic("Illegal octal character");

            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                int digits = 1;
                size_t j = i + 1;
                while (j < oldstr.size() && digits < 3 && oldstr[j] >= '0' && oldstr[j] <= '7') {
                    digits++;
                    j++;
                }
                int value = std::stoi(oldstr.substr(i, digits), nullptr, 8);
                newstr << static_cast<char>(value);
                i += digits- 1;
                break;
            }

            default: newstr << '\\' << ch; break;
        }
        saw_backslash = false;
    }

    return newstr.str();
}

std::tuple<llvm::APInt, BabelType> parseInt(std::string_view s, int prefix, uint8_t base) {
    size_t len = s.size() - prefix;

    if (s.find('_') != std::string::npos) {
        len -= 2;
    } else if (s.find_first_of("BbSsIiLlCc", prefix) != std::string::npos) {
        len--;
    }

    char suffix = s.substr(prefix + len).empty() ? '\0' : s.substr(prefix + len).back();
    unsigned bitWidth;
    switch (suffix) {
        case 'B': case 'b': bitWidth = 8; break;
        case 'S': case 's': bitWidth = 16; break;
        case 'L': case 'l': bitWidth = 64; break;
        case 'C': case 'c': bitWidth = 128; break;
        default : bitWidth = 32; break;
    }

    return std::make_tuple(llvm::APInt(bitWidth, s.substr(prefix, len), base), BabelType::IntN(bitWidth));
}

static const llvm::fltSemantics& fpSemanticsFromSuffix(char suffix) {
    switch (suffix) {
        case 'H': case 'h': return llvm::APFloatBase::IEEEhalf();
        case 'F': case 'f': return llvm::APFloatBase::IEEEsingle();
        case 'D': case 'd': return llvm::APFloatBase::IEEEdouble();
        case 'Q': case 'q': return llvm::APFloatBase::IEEEquad();
        default:            return llvm::APFloatBase::IEEEsingle();
    }
}

BabelType fpTypeFromSuffix(char suffix) {
    switch (suffix) {
        case 'H': case 'h': return BabelType::Float16();
        case 'F': case 'f': return BabelType::Float32();
        case 'D': case 'd': return BabelType::Float64();
        case 'Q': case 'q': return BabelType::Float128();
        default:            return BabelType::Float32();
    }
}

// to be added: uint, overloads, optionals
enum class OpKind {
    AddInt,
    AddFloat,
    AddPtr,
    SubInt,
    SubFloat,
    SubPtr,
    PtrDiff,
    MulInt,
    MulFloat,
    MulBool,
    Div,
    IDiv,
    RemInt,
    RemFloat,
    PowerInt,
    PowerFloat,
    PowerFloatInt,
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
    LShr,
    EqInt,
    EqFloat,
    NeInt, 
    NeFloat,
    LtInt,
    LtFloat,
    LeInt,
    LeFloat,
    GtInt,
    GtFloat,
    GeInt,
    GeFloat,
    LogicalAnd,
    LogicalOr,
    Not,
    Neg,
    FNeg,
    Id,
    PreInc,
    PostInc,
    PreDec,
    PostDec,
    PrePtrInc,
    PostPtrInc,
    PrePtrDec,
    PostPtrDec
};

OpKind getOperation(std::string_view op, BabelType left, BabelType right) {
    bool isFloatCompatible = (isBabelFloat(left) && isBabelFloat(right)) || (isBabelFloat(left) && isBabelInteger(right)) || (isBabelInteger(left) && isBabelFloat(right));
    bool isPointerArithmetic = (left.isPointer() && isBabelInteger(right)) || (isBabelInteger(left) && right.isPointer());

    if (op == "+") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::AddInt;
        
        if (isFloatCompatible)
            return OpKind::AddFloat;
        
        if (isPointerArithmetic)
            return OpKind::AddPtr;
    } else if (op == "-") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::SubInt;
        
        if (isFloatCompatible)
            return OpKind::SubFloat;

        if (isPointerArithmetic)
            return OpKind::SubPtr;
        
        if (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer()))
            return OpKind::PtrDiff;
    } else if (op == "*") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::MulInt;
        
        if (isFloatCompatible)
            return OpKind::MulFloat;

        if (left == BabelType::Boolean() ^ right == BabelType::Boolean())
            return OpKind::MulBool;
    } else if (op == "/") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || isFloatCompatible)
            return OpKind::Div;
    } else if (op == "//") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::IDiv;
    } else if (op == "%") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::RemInt;
        
        if (isFloatCompatible)
            return OpKind::RemFloat;
    } else if (op == "<<") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::Shl;
    } else if (op == ">>") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::Shr;
    } else if (op == ">>>") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::LShr;
    } else if (op == "|") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::BitOr;
    } else if (op == "&") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::BitAnd;
    } else if (op == "^" || op == "^^") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::BitXor;
    } else if (op == "**") {
        if (isBabelInteger(left) && isBabelInteger(right))
            return OpKind::PowerInt;
        
        if (isBabelFloat(left) && isBabelInteger(right))
            return OpKind::PowerFloatInt;
        
        if (isFloatCompatible)
            return OpKind::PowerFloat;
    } else if (op == "==") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::EqInt;
        
        if (isFloatCompatible)
            return OpKind::EqFloat;
    } else if (op == "!=") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::NeInt;
        
        if (isFloatCompatible)
            return OpKind::NeFloat;
    } else if (op == "<") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::LtInt;
        
        if (isFloatCompatible)
            return OpKind::LtFloat;
    } else if (op == "<=") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::LeInt;
        
        if (isFloatCompatible)
            return OpKind::LeFloat;
    } else if (op == ">") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::GtInt;
        
        if (isFloatCompatible)
            return OpKind::GtFloat;
    } else if (op == ">=") {
        if ((isBabelInteger(left) && isBabelInteger(right)) || (left.isPointer() && right.isPointer() && areComparablePointers(left.getPointer(), right.getPointer())))
            return OpKind::GeInt;
        
        if (isFloatCompatible)
            return OpKind::GeFloat;
    } else if (op == "&&") {
        // in the future an optional type is fine as well
        if (left == BabelType::Boolean() && right == BabelType::Boolean())
            return OpKind::LogicalAnd;
    } else if (op == "||") {
        // in the future an optional type is fine as well
        if (left == BabelType::Boolean() && right == BabelType::Boolean())
            return OpKind::LogicalOr;
    } else {
        babel_panic("Invalid binary operator %s", op.data());
    }

    babel_panic("Invalid types (%s and %s) to binary operator %s", getBabelTypeName(left).c_str(), getBabelTypeName(right).c_str(), op.data());
}

OpKind getOperation(std::string_view op, BabelType ty) {
    if (op == "!") {
        // in the future an optional type is fine as well
        if (ty == BabelType::Boolean())
            return OpKind::Not;
    } else if (op == "~") {
        // same instruction as above
        if (isBabelInteger(ty))
            return OpKind::Not;
    } else if (op == "-") {
        if (isBabelInteger(ty))
            return OpKind::Neg;

        if (isBabelFloat(ty))
            return OpKind::FNeg;
    } else if (op == "+") {
        if (isBabelInteger(ty) && isBabelFloat(ty))
            return OpKind::Id;
    } else if (op == "pre++") {
        if (isBabelInteger(ty))
            return OpKind::PreInc;
        
        if (ty.isPointer())
            return OpKind::PrePtrInc;
    } else if (op == "pre--") {
        if (isBabelInteger(ty))
            return OpKind::PreDec;
        
        if (ty.isPointer())
            return OpKind::PrePtrDec;
    } else if (op == "post++") {
        if (isBabelInteger(ty))
            return OpKind::PostInc;
        
        if (ty.isPointer())
            return OpKind::PostPtrInc;
    } else if (op == "post--") {
        if (isBabelInteger(ty))
            return OpKind::PostDec;
        
        if (ty.isPointer())
            return OpKind::PostPtrDec;
    } else {
        babel_panic("Invalid unary operator %s", op.data());
    }

    babel_panic("Invalid type (%s) to unary operator %s", getBabelTypeName(ty).c_str(), op.data());
}

#endif /* TOOLS_HPP */