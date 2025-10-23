#include <iostream>
#include <string>
#include <list>
#include <map>
#include <ranges>
#include <algorithm>
#include <regex>
#include <stdexcept>

class Token {
private:
    std::string type;
    std::string value;

public:
    Token(const std::string& type, const std::string& value) : type(type), value(value) {}

    std::string getType() const {
        return type;
    }

    std::string getValue() const {
        return value;
    }

    friend std::ostream& operator<<(std::ostream &s, const Token &token) {
        if (token.getValue() != "") {
            return s << token.getType() << " : " << token.getValue();    
        } else {
            return s << token.getType();
        }
    }

    Token& operator=(const Token& tok) = default;
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

    public:
        Lexer (std::string file_name, std::list<std::pair<std::string, std::string>> token_specs) : file_name(file_name), token_specs(token_specs) {
            //pos = Position(0, -1, -1, file_name, text);
            current_char = (char) 0;
            advance();
        }

        void advance () {
            pos.advance(current_char);
            current_char = pos.getInd() < text.size() ? text[pos.getInd()] : (char) 0;
        }

        std::vector<Token> tokenize(std::string input_stream) const {
            std::vector<Token> tokens;
            
            while (!input_stream.empty()) {
                bool matched = false;
                for (const auto& [token_type, pattern] : token_specs) {
                    std::regex regex_pattern("^" + pattern);
                    std::smatch re;
                    
                    if (regex_search(input_stream, re, regex_pattern)) {
                        std::string match = re[0];
                        if (match.size() == 0) break; //fixing wrong matches
                        tokens.emplace_back(token_type, match);
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

        static std::vector<Token> insertSemicolons(std::vector<Token>& tokens) {
            auto subv = std::ranges::unique(tokens, [](const Token& a, const Token& b) { return a.getType() == "NEWLINE" && b.getType() == "NEWLINE"; });
            tokens.erase(subv.begin(), tokens.end());

            for (size_t i = 1; i < tokens.size() - 1; ++i) {
                if (tokens[i].getType() == "NEWLINE" && isLineTerminating(tokens[i-1].getType()) && !isContinuation(tokens[i+1].getType())) {
                    tokens[i] = Token("SEMICOLON", ";");
                }
            }

            std::erase_if(tokens, [](const Token& tok){ return tok.getType() == "NEWLINE"; });

            return tokens;
        }

        static bool isLineTerminating(std::string_view type) {
            return type == "VAR" || type == "TYPE" || 
                    type == "INTEGER" || type == "FLOATING_POINT" || type == "CHAR" || type == "STRING" || type == "BOOL" || type == "NULL" ||
                    type == "BREAK" || type == "CONTINUE" || type == "RETURN" || type == "NOOP" || type == "FALLTHROUGH" || type ==  "END" ||
                    type == "INCREMENT" || type == "DECREMENT" || type == "RPAREN" || type == "RBRACE";
        }
        
        static bool isContinuation(std::string_view type) {
            // method chaining, also consider adding else (and similar), opening "brace" things like if condition then, pipe operator
            return type == "DOT";
        }
};
