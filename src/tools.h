#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>

template <typename T, typename Container>
int indexOf(const T& element, const Container& container) {
    if (auto it = std::ranges::find(container, element); it != container.end()) {
        return std::distance(container.begin(), it);
    }

    return -1;
}

template <typename Container>
bool includes(Container list1, Container list2) {
    for (const auto& elmnt : list1) {
        if (indexOf(elmnt, list2) < 0){
            return false;
        }
    }

    return true;
}

template <typename Container>
bool includeEachOther(Container list1, Container list2) {
    return includes(list1, list2) && includes(list2, list1);
}

// leave for clarity, replace later
template <typename K, typename T, typename Hash = std::hash<K>, typename Compare = std::equal_to<>>
std::list<T> getOrCreateArray(std::unordered_map<K, std::list<T>, Hash, Compare>& dict, const K& key) {
    return dict[key];
}

template<typename Container>
requires std::same_as<typename Container::value_type, std::string>
Container trimElements(const Container& container) {
    Container result = {};

    for (const std::string& elmnt : container) {
        result.push_back(boost::trim_copy(elmnt));
    }

    return result;
}

template <template <typename> class Container>
Container<std::string> splitString(std::string_view input, std::string_view delimiter) {
    Container<std::string> result;
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
template <typename T, typename Container>
bool isElement(T elmnt, const Container& container) {
    return std::ranges::find(container, elmnt) != container.end();
}

template <typename T, typename Container>
bool addUnique(T elmnt, Container& container) {
    if (!isElement(elmnt, container)) {
        container.push_back(elmnt);

        return true;
    }

    return false;
}

template <typename Container>
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

#endif /* TOOLS_HPP */