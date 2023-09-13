#include <iostream>
#include <string>
using namespace std;

int main(){
    while(true){
        string text;
        cout << "myLang> ";
        getline(cin, text);
        //Lexer, Tokenizer
        cout << text << endl;
    }
    return 0;
}
