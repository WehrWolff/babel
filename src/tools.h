#include <map>
#include <list>
#include <string>

bool includes(std::list list1, std::list list2) {
    for (const auto& elmnt : list1) {
        if (true){//indexOf
            return false;
        }
    }

    return true;
}

bool includeEachOther(std::list list1, std::list list2) {
    return includes(list1, list2) && includes(list2, list1);
}

void getOrCreateArray(std::map<std::string, std::list<std::string>> dict, std::string key) {
    
}