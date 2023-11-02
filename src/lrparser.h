#include <algorithm>
#include <list>
#include <map>
#include <regex>
#include <stack>
#include <string>
#include <variant>

class SyntaxError : public std::exception {
private:
    std::string error_log;

public:
    SyntaxError(const std::string& msg, const std::string& file, int line) {
        error_log = file + ": SyntaxError: " + msg + " at line " + std::to_string(line);
    }

    const char* what() const noexcept override {
        return error_log.c_str();
    }
};

// Define a macro to create the exception with file and line
#define SyntaxError(msg) throw SyntaxError(msg, __FILE__, __LINE__)

typedef void (*standalone_function_t)();

/* class NonTerminal {
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
}; */

class Rule;

class Grammar {
    public:
        std::list<std::string> alphabet;
        std::list<std::string> nonterminals;
        std::list<std::string> terminals;
        std::list<NonTerminal> rules;
        std::string text;
        //firsts, follows
        //check all types

        void initializeRulesAndAlphabetAndNonterminals(){
            std::list<std::string> lines; //text.split("\n")

            for (int i = 0; i < lines.size(); i++) {
                std::string line = lines[i]; //trim

                if (line != '') { // possibly remove in the future
                    Rule rule = new Rule(this, line); // should use unique_pointr here later

                    rules.push_back(rule);
                }
            }
        }

        Grammar(std::list<NonTerminal> rules) : rules(rules) {
            initializeRulesAndAlphabetAndNonterminals();
            initializeAlphabetAndTerminals();
            initializeFirsts();
            initializeFollows();
        }

        std::list<Rule> getRulesForNonterminal(std::string nonterminal) {//string?
            std::list<Rule> result = {};

            for (const Rule& rule : rules) {
                if (nonterminal == rule.nonterminal) {
                    result.push_back(rule);
                }
            }

            return result;
        }

        std::list<std::string> splitAndTrim(std::string in, std::string trim) {
            std::list<std::string> result = {};

            return result;
        }

        /* bool validate(std::list symbols, Token nextTok) {
            std::string token_type = nextTok.getType();
            std::string grammarQuery;
            for (auto symbol : symbols) {
                std::string name = typeid(symbol) == typeid(Token) ? symbol.getType() : symbol.getName();                
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

        void reduce(std::stack symbols) {    
            std::string grammarQuery;
            for (auto symbol : symbols) {
                std::string name = typeid(symbol) == typeid(Token) ? symbol.getType() : symbol.getName();
                grammarQuery.append(name + ' ');
            }
            grammarQuery.pop_back();

            for (NonTerminal rule : rules) {
                for (std::string s : rule.getMappedStrings()) {
                    if (grammarQuery.find(s) != std::string::npos) {
                        NonTerminal reduced = new NonTerminal(rule.getName(), nullptr);
                        int reductions = std::ranges::count(s, ' ') + 1;
                        while (reductions != 0) {
                            symbols.pop();
                            reductions--;
                        }
                        symbols.push(reduced);
                    }
                }
            }
            // we know that we tried to shift tokens before reduce which didnt work
            // we know that none of our grammar rules matches the symbol stack
            // therefor the syntax isnt valid
            throw SyntaxError("");
        } */
};

class Rule {
    public:
        Grammar& grammar;
        int index;
        std::string nonterminal;
        std::list<std::string> pattern;
        std::list<std::string> development;        
        
        Rule(Grammar& grammar, std::string text) : grammar(grammar), index(grammar.rules.size()) {
            size_t arrowIndex = text.find("->");
            nonterminal = text.substr(0, arrowIndex);
            pattern = grammar.splitAndTrim(nonterminal, " ");
            std::string developmentStr = text.substr(arrowIndex + 2);
            development = grammar.splitAndTrim(developmentStr, " ");
        }
};

class BasicItem {
    public:
        Rule& rule;
        int dotIndex; //int?
        std::list<std::string> lookAheads; //std::string?
    
        BasicItem (Rule& rule, int dotIndex) : rule(rule), dotIndex(dotIndex) {}

        void addUniqueTo(Object items) {
            return addUniqueUsingEquals(this, items);
            // add function
        }
};

/* class BasicLR1Item : public BasicItem {
    
}; */
