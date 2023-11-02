#include <map>
#include <list>
#include <boost/algorithm/string.hpp>
#include <string>
#include <sstream>

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
std::list<std::string> getOrCreateArray(std::map<std::string, std::list<std::string>> dict, std::string key) {
    return dict[key];
}

std::list<std::string> trimElements(std::list<std::string> list) {
    std::list<std::string> result = {};

    for (const std::string& elmnt : list) {
        result.push_back(boost::trim_copy(elmnt));
    }

    return result;
}

std::list<std::string> splitString(const std::string& input, char delimiter) {
    std::list<std::string> result;
    std::istringstream ss(input);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        result.push_back(token);
    }

    return result;
}

// leave for clarity, potentially replace later
template <typename T>
bool isElement(T elmnt, std::list<T> list) {
    return std::find(list.begin(), list.end(), elmnt) != list.end();
}

template <typename T>
bool addUnique(T elmnt, std::list<T> list) {
    if (!isElement(elmnt, list)) {
        list.push_back(elmnt);

        return true;
    }

    return false;
}
