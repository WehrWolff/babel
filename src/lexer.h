#include <iostream>
#include <string>
#include <list>
#include <map>
#include <regex>
#include <stdexcept>

class Internal_Token {
    private:
        std::string tok;
        std::string value;

    public:
        Internal_Token (std::string _tok, std::string _value = "") {
            tok = _tok;
            value = _value;
        }

        constexpr std::string getTok () {
            return tok;
        }

        constexpr std::string getValue () {
            return value;
        }
};

std::ostream& operator<< (std::ostream &s, const Internal_Token &token) {
    if (token.getValue() != "") {
        return s << token.getTok() << ":" << token.getValue();    
    } else {
        return s << token.getTok();
    }
}

class Token {
    private:
        std::string tok;
        std::string regex;
    
    public:
        Token (std::string _tok, std::string _regex) {
            tok = _tok;
            regex = _regex;
        }

        constexpr std::string getTok () {
            return tok;
        }

        void setRegex (std::string _regex) {
            regex = _regex;
        }

};

class Position {
    private:
        int line;
        int col;
        int ind;

        std::string file_name;
        std::string text;

    public:
        Position () = default;
        Position (int _line, int _col, int _ind, std::string _file_name, std::string _text) {
            line = _line;
            col = _col;
            ind = _ind;
            file_name = _file_name;
            text = _text;
        }

        void advance (char current_char) {
            col++;
            ind++;
            
            if (current_char == '\n') {
                line++;
                col = 0;
            }            
        }

        constexpr int getInd () {
            return ind;
        }
};

class Lexer {
    private:
        std::string file_name;
        std::string text;
        
        Position pos;

        char current_char;

        //std::regex_match("subject", std::regex("\s"))
        std::list<Token> tokens;
        //own class for internal tokens because of regex?

        const std::string linechange = "\n";

        std::string ignore = " \t";
        std::string line_comment = "~";
        std::string block_comment = "===";

        std::list<std::string> literals; //{"+", "-", "*", "/"};
        std::map<std::string, std::string> keywords; // name -> regex        

    public:
        Lexer (std::string _file_name, std::string _text) {
            file_name = _file_name;
            text = _text;
            pos = Position(0, -1, -1, _file_name, _text);
            current_char = (char) 0;
            advance();
        }

        void setIgnore (std::string _ignore) {
            ignore = _ignore;
        }

        void setLineComment (std::string _line_comment) {
            line_comment = _line_comment;
        }

        void setBlockComment (std::string _block_comment) {
            block_comment = _block_comment;
        }
        
        void setKeywords (std::map<std::string, std::string> _keywords) {
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

        void advance () {
            pos.advance(current_char);
            current_char = pos.getInd() < text.size() ? text[pos.getInd()] : (char) 0;
        }

        std::list<Internal_Token> tokenize() {
            std::list<Internal_Token> tokens;
            /*for (Token const& tk : tokens) {
                
                if (tk.getValue() == "") {
                    
                }
            }*/

            while (current_char != (char) 0) {
                switch (current_char) {
                    case '+':
                        tokens.push_back(Internal_Token("PLUS"));
                        break;
                    case '-':
                        tokens.push_back(Internal_Token("MINUS"));
                        break;
                    case '*':
                        tokens.push_back(Internal_Token("MULTIPLY"));
                        break;
                    case '/':
                        tokens.push_back(Internal_Token("DIVIDE"));
                        break;
                    default:
                        return {};
                }
            }

            return tokens;
        }
        
};

std::list<Internal_Token> run (std::string file_name, std::string text) {
    Lexer lexer = Lexer(file_name, text); //construct with file name, text
    return lexer.tokenize();
}
