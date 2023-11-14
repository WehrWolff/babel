#include <boost/algorithm/string.hpp>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>

template <typename T>
typename std::list<T>::iterator findIndex(std::list<T>& myList, const T& value) {
    typename std::list<T>::iterator it = myList.begin();
    size_t index = 0;

    while (it != myList.end()) {
        if (*it == value) {
            return it; // Return the iterator pointing to the element
        }
        ++it;
        ++index;
    }

    return myList.end(); // Element not found
}

template <typename T>
bool includes(std::list<T> list1, std::list<T> list2) {
    for (const auto& elmnt : list1) {
        if (findIndex(list2, elmnt) == list2.end()){
            return false;
        }
    }

    return true;
}

template <typename T>
bool includeEachOther(std::list<T> list1, std::list<T> list2) {
    return includes(list1, list2) && includes(list2, list1);
}

/* template <typename T>
bool includesUsingEquals(std::list<T> list1, std::list<T> list2) {
    for (const auto& elmnt : list1) {
        if (indexOfUsingEquals(elmnt, list2)) {
            return false;
        }
    }

    return true;
}

template <typename T>
bool includeEachOtherUsingEquals(std::list<T> list1, std::list<T> list2) {
    return includesUsingEquals(list1, list2) && includesUsingEquals(list2, list1);
} */

// leave for clarity, replace later
template <typename T>
std::list<T> getOrCreateArray(std::map<std::string, std::list<T>> dict, std::string key) {
    return dict[key];
}

std::list<std::string> trimElements(std::list<std::string> list) {
    std::list<std::string> result = {};

    for (const std::string& elmnt : list) {
        result.push_back(boost::trim_copy(elmnt));
    }

    return result;
}

std::list<std::string> splitString(const std::string& input, const std::string& delimiter) {
    std::list<std::string> result;
    size_t startPos = 0;
    size_t foundPos;

    while ((foundPos = input.find(delimiter, startPos)) != std::string::npos) {
        result.push_back(input.substr(startPos, foundPos - startPos));
        startPos = foundPos + delimiter.length();
    }

    // Add the last part of the string
    result.push_back(input.substr(startPos));

    return result;
}

// leave for clarity, potentially replace later
template <typename T>
bool isElement(T elmnt, std::list<T>& list) {
    return std::find(list.begin(), list.end(), elmnt) != list.end();
}

template <typename T>
int indexOf(T element, std::list<T> list) {
    for (int i = 0; i < list.size(); i++) {
        if (element == getElement(i, list)) {
            return i;
        }
    }

    return -1;
}

template <typename T>
bool addUnique(T elmnt, std::list<T> list) {
    if (!isElement(elmnt, list)) {
        list.push_back(elmnt);

        return true;
    }

    return false;
}

template <typename T>
T getElement(int index, std::list<T> list) {
    auto list_begin = list.begin();    
    return *std::next(list_begin, index);
}

template <typename T>
std::list<T> slice(std::list<T> arr, int start = 0, std::optional<int> end = std::nullopt) {
    // Check for valid start and end indices
    if (start < 0) {
        start += arr.size();
    }

    int actualEnd = (end == std::nullopt) ? arr.size() : end.value();

    if (actualEnd < 0) {
        actualEnd += arr.size();
    }

    if (actualEnd > arr.size()) {
        actualEnd = arr.size();
    }

    // Create a new list to store the sliced elements
    std::list<T> result;
    auto it = std::next(arr.begin(), start);

    while (it != std::next(arr.begin(), actualEnd)) {
        result.push_back(*it);
        ++it;
    }

    return result;
}