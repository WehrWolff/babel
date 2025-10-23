#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>

#include "util.hpp"

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

#endif /* TOOLS_HPP */