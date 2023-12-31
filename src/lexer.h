#include <iostream>
#include <string>
#include <list>
#include <map>
#include <regex>
#include <stdexcept>

class Token {
    private:
        const std::string type;
        const std::string value;

    public:
        Token (std::string _type, std::string _value) : type(_type), value(_value) {}

        std::string getType () const {
            return type;
        }

        std::string getValue () const {
            return value;
        }
};

std::ostream& operator<< (std::ostream &s, const Token &token) {
    if (token.getValue() != "") {
        return s << token.getType() << " : " << token.getValue();    
    } else {
        return s << token.getType();
    }
}

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

        int getInd () const {
            return ind;
        }
};

class Lexer {
    private:
        std::string file_name;
        std::string text;
        
        std::list<std::pair<std::string, std::string>> token_specs;

        Position pos;
        char current_char;
                
        /*const std::string linechange = "\n";
        std::string ignore = " \t";
        std::string line_comment = "~";
        std::string block_comment = "===";
        std::list<std::string> literals;
        std::map<std::string, std::string> keywords;*/

    public:
        Lexer (std::string _file_name, std::string _text) {
            file_name = _file_name;
            text = _text;
            pos = Position(0, -1, -1, _file_name, _text);
            current_char = (char) 0;
            advance();
        }
        /*
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
        */
        
        void setSpecs (std::list<std::pair<std::string, std::string>> _specs) {
            token_specs = _specs;
        }

        void advance () {
            pos.advance(current_char);
            current_char = pos.getInd() < text.size() ? text[pos.getInd()] : (char) 0;
        }

        std::list<Token> tokenize() {
            std::list<Token> tokens;
            std::string input_stream = text;
            
            while (!input_stream.empty()) {
                bool matched = false;
                for (const std::pair<std::string, std::string>& spec : token_specs) {
                    const std::string& token_type = spec.first;
                    const std::string& pattern = spec.second;

                    std::regex regex_pattern("^" + pattern);
                    std::smatch re;
                    
                    if (regex_search(input_stream, re, regex_pattern)) {
                        std::string match = re[0];
                        if (match.size()==0) break; //fixing wrong matches
                        tokens.push_back(Token(token_type, match));
                        input_stream = re.suffix();
                        matched = true;
                        break;
                    }
                }

                if (!matched) {
                    //ignore or handle errors
                    input_stream = input_stream.substr(1);
                }
            }            

            return tokens;
        }
        
};
