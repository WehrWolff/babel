#include <iostream>
#include <string>
#include "lexer.h"

int main(){
    while(true){
        std::string text;
        cout << "myLang> ";
        getline(cin, text);
        run("repl", text);
        cout << text << endl;
    }
    return 0;
}
