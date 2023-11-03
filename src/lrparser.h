#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <list>
#include <map>
#include <regex>
#include <stack>
#include <string>
#include <variant>
#include "tools.h"

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

const std::string EPSILON = "''";

class Rule;

class Grammar {
    private:
        void initializeRulesAndAlphabetAndNonterminals () {
            std::list<std::string> lines = splitString(text, "\n");

            for (const std::string& _ : lines) {
                std::string line = boost::trim_copy(_);

                if (line != "") {
                    // illegal as it isnt fully defined yet
                    // should use unique_pointr here later
                    Rule rule = new Rule(this, line);

                    rules.push_back(rule);

                    if (axiom == "") {
                        axiom = rule.nonterminal;
                    }
                    
                    addUnique(rule.nonterminal, alphabet);
                    addUnique(rule.nonterminal, nonterminals);
                }
            }
        }

        void initializeAlphabetAndTerminals () {
            for (const Rule& rule : rules) {
                for (const std::string& symbol : rule.development) {
                    if (symbol != EPSILON && !std::find(nonterminals.begin(), nonterminals.end(), symbol) == nonterminals.end()) {
                        addUnique(symbol, alphabet);
                        addUnique(symbol, terminals);
                    }
                }
            }
        }

        bool collectDevelopmentFirsts(std::list<std::string> development, std::list<std::string> nonterminalFirsts) {
            bool result = false;
            bool epsilonInSymbolFirsts = true;

            for (const std::string& symbol : development) {
                epsilonInSymbolFirsts = false;

                if (isElement(symbol, terminals)) {
                    result |= addUnique(symbol, nonterminalFirsts);
                    
                    break;
                }

                //check for loop (potentially places symbol in firsts)
                for (const std::string& first : firsts[symbol]) {
                    epsilonInSymbolFirsts |= first == EPSILON;
                    result |= addUnique(first, nonterminalFirsts);
                }

                if (!epsilonInSymbolFirsts) break;
            }

            if (epsilonInSymbolFirsts) {
                result |= addUnique(EPSILON, nonterminalFirsts);
            }

            return result;
        }

        void initializeFirsts () {
            bool notDone;

            do{
                notDone = false;

                for (const Rule& rule : rules) {
                    std::list<std::string> nonterminalFirsts = getOrCreateArray(firsts, rule.nonterminal);

                    if (rule.development.size() == 1 && rule.development.front() == EPSILON) {
                        notDone |= addUnique(EPSILON, nonterminalFirsts);
                    } else {
                        notDone |= collectDevelopmentFirsts(rule.development, nonterminalFirsts);
                    }
                }
            } while (notDone);
        }

        void initializeFollows() {
            bool notDone;

            do {
                notDone = false;

                for (const Rule& rule : rules) {
                    if (rule == rules.front()) {
                        std::list<std::string> nonterminalFollows = getOrCreateArray(follows, rule.nonterminal);
                        notDone |= addUnique ("$", nonterminalFollows);
                    }

                    for (const std::string& symbol : rule.development) {
                        if (isElement(symbol, nonterminals)) {
                            std::list<std::string> symbolFollows = getOrCreateArray(follows, symbol);
                            // slice function
                            std::list<std::string> afterSymbolFirsts;

                            for (const std::string& first : afterSymbolFirsts) {
                                if (first == EPSILON) {
                                    //check again (potentially places symbol in follows)
                                    std::list<std::string> nonterminalFollows = follows[rule.nonterminal];

                                    for (const std::string& _ : nonterminalFollows) {
                                        notDone |= addUnique(_, symbolFollows);
                                    }
                                } else {
                                    notDone |= addUnique(first, symbolFollows);
                                }
                            }
                        }
                    }
                }
            } while (notDone);
        }

    public:
        std::list<std::string> alphabet;
        std::list<std::string> nonterminals;
        std::list<std::string> terminals;
        std::list<Rule> rules;
        std::string text;
        std::map<std::string, std::list<std::string>> firsts;
        std::map<std::string, std::list<std::string>> follows;
        std::string axiom;
        //check all types

        Grammar(std::string text) {
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

        std::list<std::string> getSequenceFirsts(std::list<std::string> sequence) {
            std::list<std::string> result = {};
            bool epsilonInSymbolFirsts = true;

            for (const std::string& symbol : sequence) {
                epsilonInSymbolFirsts = false;

                if (std::find(terminals.begin(), terminals.end(), symbol) != terminals.end()) {
                    addUnique(symbol, result);

                    break;
                }
                
                //check for loop (potentially places symbol in firsts)
                for (const std::string& first : firsts[symbol]) {
                    epsilonInSymbolFirsts |= first == EPSILON;
                    addUnique(first, result);
                }
                // check line below again
                epsilonInSymbolFirsts |= firsts[symbol].size() == 0

                if (!epsilonInSymbolFirsts) break;                
            }

            if (epsilonInSymbolFirsts) addUnique(EPSILON, result);

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
            std::list<std::string> split = splitString(text, "->");
            nonterminal = boost::trim_copy(split.front());
            pattern = trimElements(splitString(nonterminal, " "));
            development = trimElements(splitString(boost::trim_copy(split.back()), " "));
        }
};

/* class BasicItem {
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

class BasicLR1Item : public BasicItem {
    
};

class Item : public BasicLR1Item {
    public:
        const std::string grammarType = "LR(1)";

        Item (Rule& rule, int dotIndex) : BasicLR1Item(rule, dotIndex) {}
}; */
