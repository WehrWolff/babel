#include <iostream>
#include <string>
#include <list>
#include <regex>
#include <stdexcept>

class Lexer {
    private:
        //std::regex_match("subject", std::regex("\s"))

        const std::string linechange = "\n";

        std::string ignore = " \t";
        std::string line_comment = "~";
        std::string block_comment = "===";

        std::list<string> literals; //{"+", "-", "*", "/"};

        enum Token {
            TOKEN_NUMBER = -1, //"NUMBER"
            TOKEN_STRING = -2, //"STRING"
            TOKEN_VAR = -3, //"VAR"
        
            TOKEN_IF = -4, //"IF"
            TOKEN_ELSE = -5 //"ELSE"

        };

    public:
        void setIgnore (_ignore) {
            ignore = _ignore;
        }

        void setLineComment (_line_comment) {
            line_comment = _line_comment;
        }

        void setBlockComment (_block_comment) {
            block_comment = _block_comment;
        }

        void setLiterals (std::list<string> _literals) {
            for (std::string literal : _literals) {
                if (literal.size() != 1) {
                    throw std::invalid_argument("literals must be of size 1");
                }
            }
            literals = _literals;
        }
};

class Token {
    private:
        std::string tok;
        auto value;

        std::string regex;
    
    public:
        std::string getTok () {
            return tok;
        }

        auto getValue () {
            return value;
        }

        std::ostream& operator<< (std::ostream &s, const Token &token) {
            return s << token.getTok() << ":" << token.getValue();
        }

};

/* class Position {

}; */