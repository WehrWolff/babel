#include <iostream>
#include <string>
#include <list>
#include <map>
#include <regex>
#include <stdexcept>

class Lexer {
    private:
        //std::regex_match("subject", std::regex("\s"))
        std::list<Token> tokens;

        const std::string linechange = "\n";

        std::string ignore = " \t";
        std::string line_comment = "~";
        std::string block_comment = "===";

        std::list<std::string> literals; //{"+", "-", "*", "/"};
        std::map<std::string, std::string> keywords; // name -> regex        

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

        /*
        void tokenize() {
            for (Token const& tk : tokens) {
                
                if (tk.getValue() == "") {
                    
                }
            }          
        }
        */
};

class Token {
    private:
        std::string tok;
        std::string value;

        std::string regex;

        Position index;
    
    public:
        constexpr std::string getTok () {
            return tok;
        }

        constexpr std::string getValue () {
            return value;
        }

        void setRegex (std::string _regex) {
            regex = _regex;
        }

};

std::ostream& operator<< (std::ostream &s, const Token &token) {
    if (token.getValue() != "") {
        return s << token.getTok() << ":" << token.getValue();    
    } else {
        return s << token.getTok();
    }
}

class Position {
    private:
        int line = 0;
        int col = -1;
        int ind = -1;

        std::string file_name;        

        std::string text;
        char current_char = (char) 0;        

        //advance on init

    public:
        void advance () {
            col++;
            ind++;
            
            if (current_char == '\n') {
                line++;
                col = 0;
            }
            
            col++;
            current_char = col < text.size() ? text[col] : (char) 0;
        }

        char getCurrentChar () {
            return current_char;
        }
};
