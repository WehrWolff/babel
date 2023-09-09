#include <string>
#include <list>
#include <regex>
#include <stdexcept>

class Lexer{
    private:
        //std::regex_match("subject", std::regex("\s"))
        const string linechange = "\n";
        string ignore = " \t";
        string line_comment = "~";
        string block_comment = "===";

        std::list<string> _literals; //{"+", "-", "*", "/"};

        enum Token{
            TOKEN_NUMBER = -1, //"NUMBER"
            TOKEN_STRING = -2, //"STRING"
            TOKEN_VAR = -3, //"VAR"
        
            TOKEN_IF = -4, //"IF"
            TOKEN_ELSE = -5 //"ELSE"

        };

    public:
        void setLiterals(std::list<string> literals){
            for (string literal : literals) {
                if (literal.size() != 1) {
                    throw std::invalid_argument("literals must be of size 1");
                }
            }
            _literals = literals;
        }
};