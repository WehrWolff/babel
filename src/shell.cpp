#include "lexer.h"
using namespace std;

template<typename T>
ostream& operator<<(ostream& os, const list<T>& myList) {
    os << "{";
    if (!myList.empty()) {
        auto it = myList.begin();
        os << *it;
        ++it;
        for (; it != myList.end(); ++it) {
            os << ", " << *it;
        }
    }
    os << "}";
    return os;
}

int main(){
    while(true){
        string text;
        cout << "myLang> ";
        getline(cin, text);
        list<Token> print = run("repl", text);
        cout << print << endl;
    }
    return 0;
}
