#include <list>
#include <map>
#include <regex>
#include <stack>
#include <string>
#include <variant>

typedef void (*standalone_function_t)();

class NonTerminal {
    private:
        const std::string name;
        std::map<std::string, standalone_function_t> mapping;

    public:
        NonTerminal(std::string name, std::map<std::string, std::string> mapping) : name(name), mapping(mapping) {}

        std::string getName () const {
            return name;
        }

        std::map<std::string, standalone_function_t> getMapping () {
            return mapping;
        }

        std::list<std::string> getMappedStrings () {
            std::list<std::string> key;
            for(std::map<std::string, standalone_function_t>::iterator it = mapping.begin(); it != mapping.end(); ++it) {
                key.push_back(it->first);           
            }
            return key;
        }

        void trigger(std::string function) {
            auto x = mapping.find(function);
            
            if (x != mapping.end()) {
                result = x -> second(); // Call the stored function directly
            }
        }
};

class Grammar {
    private:
        std::list<NonTerminal> rules;

    public:
        Grammar(std::list<NonTerminal> rules) : rules(rules) {}

        bool validate(std::list symbols, Token nextTok) {
            std::string token_type = nextTok.getTok();
            std::string grammarQuery;
            for (auto symbol : symbols) {
                std::string name = typeid(symbol) == typeid(Token) ? symbol.getTok() : symbol.getName();                
                grammarQuery.append(name + ' ');
            }
            grammarQuery.append(token_type);

            for (NonTerminal rule : rules) {
                for (std::string s : rule.getMappedStrings()) {
                    if (s.rfind(grammarQuery, 0) == 0) {
                        return true;
                    }
                }
            }
            return false;
        }
};

class Parser {
    private:
        std::list<Token> tokens;

    public:
        Parser(std::list<Token> tokens) : tokens(tokens) {}

        void parse () {
            std::stack symbols;
            shift(tokens, symbols);

            do {
                // possibly remove block
                auto top = symbols.top();
                if (typeid(top) == typeid(Token)) {
                    reduce(symbols);
                    continue;
                }
                // possibly remove block
                                
                Token nextTok = tokens.front();
                if (validateGrammar(symbols, nextTok)) {
                    shift(tokens, symbols);
                } else {
                    reduce(symbols);
                }
            } while (!tokens.empty() && !symbols.empty());
        }
};

void shift(std::list<Token> tokens, std::stack symbols) {
    symbols.push(tokens.front());
    tokens.pop_front();
}

void updateTop(std::stack symbols, std::variant<Token, NonTerminal> elmnt) {
    symbols.pop();
    symbols.push(elmnt);
}

void reduce(std::stack symbols) {    
    std::string grammarQuery;
    for (auto symbol : symbols) {
        std::string name = typeid(symbol) == typeid(Token) ? symbol.getTok() : symbol.getName();
        grammarQuery.append(name + ' ');
    }
    grammarQuery.pop_back();
}
