#include <iostream>
#include <string>
#include <list>
#include <map>
#include <regex>
#include <stdexcept>

class Lexer {
    private:
        //std::regex_match("subject", std::regex("\s"))

        const std::string linechange = "\n";

        std::string ignore = " \t";
        std::string line_comment = "~";
        std::string block_comment = "===";

        std::list<std::string> literals; //{"+", "-", "*", "/"};
        std::map<std::string, std::string> keywords; // name -> regex

        enum Token {
            TOKEN_NUMBER = -1, //"NUMBER"
            TOKEN_STRING = -2, //"STRING"
            TOKEN_VAR = -3, //"VAR"
        
            TOKEN_IF = -4, //"IF"
            TOKEN_ELSE = -5 //"ELSE"

        };

    public:
        void setIgnore (std::string _ignore) {
            ignore = _ignore;
        }

        void setLineComment (std::string _line_comment) {
            line_comment = _line_comment;
        }

        void setBlockComment (std::string _block_comment) {
            block_comment = _block_comment;
        }
        
        void setKeywords (std::list<std::string> _keywords) {
            keywords = _keywords;
        }

        void setLiterals (std::list<std::string> _literals) {
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
        std::string value;

        std::string regex;
    
    public:
        constexpr std::string getTok () {
            return tok;
        }

        constexpr std::string getValue () {
            return value;
        }

};

std::ostream& operator<< (std::ostream &s, const Token &token) {
    if (token.getValue() != "") {
        return s << token.getTok() << ":" << token.getValue();    
    } else {
        return s << token.getTok();
    }
}

/* class Position {

}; */
