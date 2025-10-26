#ifndef LRPARSER_HPP
#define LRPARSER_HPP

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <cassert>
#include <fstream>
#include <format>
#include <list>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>
#include "tools.h"
#include "platform.h"
#include "lexer.h"
#include "ast_builder.h"
#include "util.hpp"

#include <iostream>
#include <queue>

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& myList) {
    os << "{";
    if (!myList.empty()) {
        auto it = myList.begin();
        os << *it;
        ++it;
        for (; it != myList.end(); ++it) {
            os << ", " << *it;
        }
    }
    os << "}";
    return os;
}

template<typename K, typename V, typename Hash = std::hash<K>, typename Compare = std::equal_to<>>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<K, V, Hash, Compare>& myMap) {
    os << "{";
    if (!myMap.empty()) {
        auto it = myMap.begin();
        os << it->first << ": " << it->second;
        ++it;
        for (; it != myMap.end(); ++it) {
            os << ", " << it->first << ": " << it->second;
        }
    }
    os << "}";
    return os;
}

class SyntaxError : public std::exception {
private:
    std::string error_log;

public:
    SyntaxError(const std::string& msg, const std::string& file, const int line) {
        error_log = std::format("{}: SyntaxError: {} at line {}", file, msg, line);
    }

    const char* what() const noexcept override {
        return error_log.c_str();
    }
};

// Define a macro to create the exception with file and line
#define SyntaxError(msg) throw SyntaxError(msg, __FILE__, __LINE__)

const std::string EPSILON = "''";

class Grammar;
class Rule {
public:
    const Grammar* grammar;
    int index;
    std::string nonterminal;
    std::list<std::string> pattern;
    std::vector<std::string> development;
    
    Rule() = default;
    Rule(const Grammar* grammar, const std::string& text);
    
    bool operator==(const Rule& that) const {
        if (nonterminal != that.nonterminal) {
            return false;
        }
    
        if (development.size() != that.development.size()) {
            return false;
        }
    
        if (development != that.development) {
            return false;
        }
    
        return true;
    }  
    
    friend std::ostream& operator<<(std::ostream& os, const Rule& rule) {
        os << rule.nonterminal << " -> ";
        if (!rule.development.empty()) {
            auto it = rule.development.begin();
            os << *it;
            ++it;
            for (; it != rule.development.end(); ++it) {
                os << " " << *it;
            }
        }
        return os;
    }
};

class Grammar {
private:
    void initializeRulesAndAlphabetAndNonterminals(const std::string& text);
    void initializeAlphabetAndTerminals();
    bool collectDevelopmentFirsts(const std::vector<std::string>& development, std::list<std::string>& nonterminalFirsts);
    void initializeFirsts();
    void initializeFollows();

public:
    std::list<std::string> alphabet;
    std::list<std::string> nonterminals;
    std::list<std::string> terminals;
    std::vector<Rule> rules = {};
    std::string text;
    mutable std::unordered_map<std::string, std::list<std::string>, TransparentStringHash, std::equal_to<>> firsts;
    std::unordered_map<std::string, std::list<std::string>, TransparentStringHash, std::equal_to<>> follows;
    std::string axiom;

    Grammar() = default;
    explicit Grammar(std::string const& text) {
        initializeRulesAndAlphabetAndNonterminals(text);
        initializeAlphabetAndTerminals();
        initializeFirsts();
        initializeFollows();
    }

    std::vector<Rule> getRulesForNonterminal(std::string_view nonterminal) const;

    std::list<std::string> getSequenceFirsts(const std::vector<std::string>& sequence) const {
        std::list<std::string> result = {};
        bool epsilonInSymbolFirsts = true;

        for (const std::string& symbol : sequence) {
            epsilonInSymbolFirsts = false;

            if (std::ranges::find(terminals, symbol) != terminals.end()) {
                addUnique(symbol, result);

                break;
            }
            
            for (const std::string& first : firsts.at(symbol)) {
                epsilonInSymbolFirsts |= first == EPSILON;
                addUnique(first, result);
            }
            
            epsilonInSymbolFirsts |= firsts.at(symbol).size() == 0;

            if (!epsilonInSymbolFirsts) break;                
        }

        if (epsilonInSymbolFirsts) addUnique(EPSILON, result);

        return result;
    }      
};

Rule::Rule(const Grammar* grammar, const std::string& text) : grammar(grammar), index(static_cast<int>(grammar->rules.size()) ) {
    std::list<std::string> split = splitString<std::list<std::string>>(text, "->");
    nonterminal = boost::trim_copy(split.front());
    pattern = trimElements(splitString<std::list<std::string>>(nonterminal, " "));
    development = trimElements(splitString<std::vector<std::string>>(boost::trim_copy(split.back()), " "));
}

void Grammar::initializeRulesAndAlphabetAndNonterminals (const std::string& grammar_text) {
    std::list<std::string> lines = splitString<std::list<std::string>>(grammar_text, "\n");

    for (const std::string& _ : lines) {
        std::string line = boost::trim_copy(_);

        if (line != "") {
            Rule rule(this, line);
            rules.push_back(rule);

            if (axiom.empty()) {
                axiom = rule.nonterminal;
            }
            
            addUnique(rule.nonterminal, alphabet);
            addUnique(rule.nonterminal, nonterminals);
        }
    }
}

void Grammar::initializeAlphabetAndTerminals () {
    for (const Rule& rule : rules) {
        for (const std::string& symbol : rule.development) {
            if (symbol != EPSILON && symbol != "$" && std::ranges::find(nonterminals, symbol) == nonterminals.end()) {
                addUnique(symbol, alphabet);
                addUnique(symbol, terminals);
            }
        }
    }
}

bool Grammar::collectDevelopmentFirsts(const std::vector<std::string>& development, std::list<std::string>& nonterminalFirsts) {
    bool result = false;
    bool epsilonInSymbolFirsts = true;
    
    for (const std::string& symbol : development) {
        epsilonInSymbolFirsts = false;

        if (isElement(symbol, terminals)) {
            result |= addUnique(symbol, nonterminalFirsts);
            
            break;
        }

        if (firsts.contains(symbol)) {
            for (const std::string& first : firsts.at(symbol)) {
                epsilonInSymbolFirsts |= first == EPSILON;
                result |= addUnique(boost::trim_copy(first), nonterminalFirsts);
            }
        }

        if (!epsilonInSymbolFirsts) break;
    }

    if (epsilonInSymbolFirsts) {
        result |= addUnique(EPSILON, nonterminalFirsts);
    }

    return result;
}

void Grammar::initializeFirsts () {
    bool notDone;

    do{
        notDone = false;

        for (Rule& rule : rules) {
            std::list<std::string> nonterminalFirsts = getOrCreateArray(firsts, rule.nonterminal);

            if (rule.development.size() == 1 && rule.development.front() == EPSILON) {
                notDone |= addUnique(EPSILON, nonterminalFirsts);
            } else {
                notDone |= collectDevelopmentFirsts(rule.development, nonterminalFirsts);
            }
            firsts[rule.nonterminal] = nonterminalFirsts;
        }
    } while (notDone);
}

void Grammar::initializeFollows() {
    bool notDone;

    do {
        notDone = false;
        
        for (const Rule& rule : rules) {
            if (rule == rules.front()) {
                std::list<std::string> nonterminalFollows = getOrCreateArray(follows, rule.nonterminal);
                notDone |= addUnique<std::string>("$", nonterminalFollows);
                follows[rule.nonterminal] = nonterminalFollows;
            }

            for (int i = 0; i < rule.development.size(); i++) {
                std::string symbol = rule.development[i];

                if (isElement(symbol, nonterminals)) {
                    std::list<std::string> symbolFollows = getOrCreateArray(follows, symbol);
                    
                    std::vector<std::string> sliced = rule.development;
                    auto end = sliced.begin();
                    std::advance(end, i + 1);
                    sliced.erase(sliced.begin(), end);
                    
                    std::list<std::string> afterSymbolFirsts = getSequenceFirsts(sliced);

                    for (const std::string& first : afterSymbolFirsts) {
                        if (first == EPSILON) {
                            std::list<std::string> nonterminalFollows = follows[rule.nonterminal];

                            for (const std::string& _ : nonterminalFollows) {
                                notDone |= addUnique(_, symbolFollows);
                            }
                        } else {
                            notDone |= addUnique(first, symbolFollows);
                        }
                    }
                    follows[symbol] = symbolFollows;
                }
            }
        }
    } while (notDone);
}

std::vector<Rule> Grammar::getRulesForNonterminal(std::string_view nonterminal) const {
    auto filtered = rules | std::views::filter([nonterminal](const Rule& rule) {
        return rule.nonterminal == nonterminal;
    });

    return std::vector<Rule>(filtered.begin(), filtered.end());
}

class UnifiedItem {
public:
    Rule rule;
    int dotIndex;
    mutable std::vector<std::string> lookAheads = {};

    UnifiedItem() = default;
    UnifiedItem(const Rule& rule, int dotIndex) : rule(rule), dotIndex(dotIndex) {
        if (rule.index == 0) {
            lookAheads.emplace_back("$");
        }
    }

    std::vector<UnifiedItem> newItemsFromSymbolAfterDot() const {
        std::vector<UnifiedItem> result = {};
        
        std::vector<Rule> nonterminalRules = {};
        if (dotIndex < rule.development.size()) {
            nonterminalRules = rule.grammar->getRulesForNonterminal(rule.development[dotIndex]);
        }

        for (const Rule& ntRule : nonterminalRules) {
            addUnique(UnifiedItem(ntRule, 0) , result);
        }

        if (result.empty()) return result;

        std::vector<std::string> newLookAheads = {};
        bool epsilonPresent = false;
        std::list<std::string> firstsAfterSymbolAfterDot = rule.grammar->getSequenceFirsts(slice(rule.development, dotIndex + 1));

        for (const std::string& first : firstsAfterSymbolAfterDot) {
            if (EPSILON == first) {
                epsilonPresent = true;
            } else {
                addUnique(first, newLookAheads);
            }
        }

        if (epsilonPresent) {
            for (const std::string& lookAhead : lookAheads) {
                addUnique(lookAhead, newLookAheads);
            }
        }

        for (UnifiedItem& item : result) {
            item.lookAheads = slice(newLookAheads, 0);
        }
        
        return result;
    }

    std::optional<UnifiedItem> newItemAfterShift() const {
        std::optional<UnifiedItem> result;
        
        if (dotIndex < rule.development.size() && rule.development[dotIndex] != EPSILON) {
            result.emplace(rule, dotIndex + 1);
        } else {
            return std::nullopt;
        }            
        
        result.value().lookAheads = slice(lookAheads, 0);            

        return result;
    }

    template<typename Container>
    requires std::same_as<typename Container::value_type, UnifiedItem>
    bool addUniqueTo(Container& items) const {
        bool result = false;

        for (const UnifiedItem& item : items) {
            if (superEquals(item)) {
                for (const std::string& lookAhead : lookAheads) {
                    result |= addUnique(lookAhead, item.lookAheads);
                }

                return result;
            }
        }

        std::inserter(items, items.end()) = *this;
        return true;
    }

    bool superEquals(const UnifiedItem& that) const {
        return rule == that.rule && dotIndex == that.dotIndex;
    }

    bool operator==(const UnifiedItem& that) const {
        return superEquals(that) && includeEachOther(lookAheads, that.lookAheads);
    }
};

inline size_t hash_value(const Rule& rule) {
    size_t seed = 0;

    boost::hash_combine(seed, rule.nonterminal);
    boost::hash_combine(seed, rule.development);

    return seed;
}

template <>
struct std::hash<UnifiedItem> {
    size_t operator()(const UnifiedItem& other) const {
        size_t seed = 0;

        boost::hash_combine(seed, other.rule);
        boost::hash_combine(seed, other.dotIndex);

        std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> uniqueLookAheads(other.lookAheads.begin(), other.lookAheads.end());
        boost::hash_combine(seed, boost::hash<std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>>{}(uniqueLookAheads));

        return seed;
    }
};

class Kernel {
public:
    int index;
    std::unordered_set<UnifiedItem> items;
    std::vector<UnifiedItem> closure;
    std::unordered_map<std::string, int, TransparentStringHash, std::equal_to<>> gotos;
    std::list<std::string> keys;

    Kernel (int index, const std::unordered_set<UnifiedItem>& items) : index(index), items(items), closure(slice(items, 0)) {}

    bool operator==(const Kernel& that) const {
        return items == that.items;
    }
};

class LRClosureTable {
public:
    Grammar& grammar;
    std::deque<Kernel> kernels;

    explicit LRClosureTable(Grammar& grammar) : grammar(grammar) {
        kernels.emplace_back(Kernel(0, {UnifiedItem(grammar.rules.front(), 0)}));

        for (int i = 0; i < kernels.size();) {
            Kernel& kernel = kernels[i];
            updateClosure(kernel);
            
            if (addGotos(kernel, kernels)) {
                i = 0;
            } else {
                ++i;
            }
        }
    }

    void updateClosure(Kernel& kernel) const {
        for (int i = 0; i < kernel.closure.size(); ++i) {
            auto it = std::next(kernel.closure.begin(), i);
            auto const& closure = *it;
            std::vector<UnifiedItem> newItemsFromSymbolAfterDot = closure.newItemsFromSymbolAfterDot();

            for (const UnifiedItem& item : newItemsFromSymbolAfterDot) {
                item.addUniqueTo(kernel.closure);
            }
        }
    }

    bool addGotos(Kernel& kernel, std::deque<Kernel>& kernel_dq) const {
        bool lookAheadsPropagated = false;
        std::unordered_map<std::string, std::unordered_set<UnifiedItem>, TransparentStringHash, std::equal_to<>> newKernels;

        for (const UnifiedItem& item : kernel.closure) {
            std::optional<UnifiedItem> newItem = item.newItemAfterShift();

            if (newItem != std::nullopt) {
                std::string symbolAfterDot = item.rule.development[item.dotIndex];

                addUnique(symbolAfterDot, kernel.keys);
                newKernels[symbolAfterDot];
                std::unordered_set<UnifiedItem>& newItems = newKernels.at(symbolAfterDot);
                newItem.value().addUniqueTo(newItems);
            }
        }
        
        for (const std::string& key : kernel.keys) {
            Kernel newKernel(static_cast<int>(kernels.size()), newKernels.at(key));
            int targetKernelIndex = indexOf(newKernel, kernels);

            if (targetKernelIndex < 0) {
                kernel_dq.push_back(newKernel);
                targetKernelIndex = newKernel.index;
            } else {
                for (const UnifiedItem& item : newKernel.items) {
                    std::unordered_set<UnifiedItem>& items = kernel_dq[targetKernelIndex].items;
                    lookAheadsPropagated |= item.addUniqueTo(items);
                }
            }
            
            kernel.gotos.try_emplace(key, targetKernelIndex);
        }
        
        return lookAheadsPropagated;
    }
};

class LRAction {
public:
    char actionType;
    int actionValue;

    LRAction() = default;
    LRAction(char actionType, int actionValue) : actionType(actionType), actionValue(actionValue) {}

    std::string toString() const {
        return std::format("{}{}", actionType == '\0' ? "" : std::string(1, actionType), actionValue);
    }

    friend std::ostream& operator<<(std::ostream& os, const LRAction& lrAction) {
        os << lrAction.toString();
        return os;
    }

    inline bool operator==(std::string_view that) const {
        return that.at(0) == actionType && std::stoi(std::string(that.substr(1))) == actionValue;
    }

    inline bool operator==(int that) const {
        return actionType == '\0' && that == actionValue;
    }
};

class State {
public:
    int index;
    std::unordered_map<std::string, LRAction, TransparentStringHash, std::equal_to<>> mapping;

    State() = default;
    explicit State(std::vector<State> const& states) : index(static_cast<int>(states.size())) {}

    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        os << state.index << ": " << state.mapping;
        return os;
    }
};

class LRTable {
public:
    Grammar grammar;
    std::vector<State> states = {};

    LRTable() = default;
    explicit LRTable(const LRClosureTable& closureTable) : grammar(closureTable.grammar) {
        for (const Kernel& kernel : closureTable.kernels) {
            State state(states);

            for (const std::string& key : kernel.keys) {                    
                int nextStateIndex = kernel.gotos.at(key);
                state.mapping.try_emplace(key, (isElement(key, closureTable.grammar.terminals) ? 's' : '\0'), nextStateIndex);
            }

            for (const UnifiedItem& item : kernel.closure) {
                if (item.dotIndex == item.rule.development.size() || item.rule.development.front() == EPSILON) {
                    for (const std::string& lookAhead : item.lookAheads) {
                        state.mapping.try_emplace(lookAhead, 'r', item.rule.index);
                    }
                }
            }
            
            states.push_back(state);
        }
    }
};

std::optional<LRAction> chooseActionElement(State& state, const std::string& token) {
    if (!state.mapping.contains(token)) {
        return std::nullopt;
    }

    return state.mapping.at(token);
}

class Parser {
public:
    LRTable lrTable;
    Parser() = default;
    explicit Parser(const LRTable& lrTable) : lrTable(lrTable) {}

    BABEL_COLD std::string retrieveMessage(const State& state, const std::string& token) const {
        std::unordered_map<std::string, LRAction, TransparentStringHash, std::equal_to<>> m = state.mapping;
        
        std::list<std::string> expected;
        for (const auto& [symbol, _] : m) {
            if (isElement(symbol, lrTable.grammar.nonterminals)) {
                expected.splice(expected.end(), lrTable.grammar.firsts.at(symbol));
            } else {
                expected.push_back(symbol);
            }
        }
        
        expected.remove(EPSILON);
        expected.sort();
        expected.unique();
        if (auto it = std::ranges::find(expected, "$"); it != expected.end()) {
            auto next = it; std::advance(next, 1);
            std::rotate(it, next, expected.end());
        }
        
        std::string msg = "Expected";
        for (std::string elmnt : expected) {
            msg += " '" + elmnt + "' or";
        }
        msg.erase(msg.rfind(' '));
        msg += " but found '" + token + "'";
        return std::regex_replace(msg, std::regex("'\\$'"), "EOF");
    }

    std::variant<TreeNode, std::string> parse(std::vector<Token> tokens) const {
        tokens.emplace_back("$", "$");
        std::stack<TreeNode> nodeStack;
        std::stack<std::variant<TreeNode, std::unique_ptr<BaseAST>>> reducedNodes;
        std::stack<int> stateStack;
        stateStack.push(0);
        int tokenIndex = 0;
        Token token = tokens[tokenIndex];
        std::string token_type = token.getType();
        State state = lrTable.states[stateStack.top()];
        std::optional<LRAction> actionElement = chooseActionElement(state, token_type);

        while (actionElement != std::nullopt && actionElement.value().toString() != "r0") {
            if (actionElement.value().actionType == 's') {
                TreeNode shiftNode;
                shiftNode.name = tokens[tokenIndex].getType();
                shiftNode.data = tokens[tokenIndex].getValue();

                nodeStack.emplace(shiftNode);
                reducedNodes.emplace(shiftNode);
                stateStack.push(actionElement.value().actionValue);
                tokenIndex++;
            } else if (actionElement.value().actionType == 'r') {
                int ruleIndex = actionElement.value().actionValue;
                Rule rule = lrTable.grammar.rules[ruleIndex];
                int removeCount = isElement(EPSILON, rule.development) ? 0 : static_cast<int>(rule.development.size());

                TreeNode newNode;
                newNode.name = rule.nonterminal;

                for (int i = 0; i < removeCount; i++) {
                    TreeNode child = nodeStack.top(); nodeStack.pop();
                    newNode.children.push_front(child);
                    stateStack.pop();
                }

                nodeStack.push(newNode);

                if (newNode.has_tokenized_child() || isElement(EPSILON, rule.development)) {
                    buildNode(reducedNodes, rule.nonterminal, removeCount);
                }
            } else {
                stateStack.push(actionElement.value().actionValue);
            }
            
            state = lrTable.states[stateStack.top()];
            token_type = (nodeStack.size() + stateStack.size()) % 2 == 0 ? nodeStack.top().name : tokens[tokenIndex].getType();
            actionElement = chooseActionElement(state, token_type);
        }

        if (actionElement != std::nullopt && actionElement.value().toString() == "r0") {
            if (!std::holds_alternative<TreeNode>(reducedNodes.top())) {
                std::deque<std::unique_ptr<BaseAST>> statement_list;
                while (!reducedNodes.empty()) {
                    statement_list.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(reducedNodes.top())));
                    reducedNodes.pop();
                }

                auto root = std::make_unique<RootAST>(std::move(statement_list));
                root->codegen()->print(llvm::errs());
                fprintf(stderr, "\n");
            }

            //return TreeNode{lrTable.grammar.axiom, std::nullopt, {reducedNodes.top()}};
            return TreeNode{lrTable.grammar.axiom, std::nullopt, {nodeStack.top()}};
            //return "Success";
        }

        return "SyntaxError: " + retrieveMessage(state, tokens[tokenIndex].getValue());
    }
};

std::string transform_string(const std::string& input_string) {
    size_t last_colon_pos;
    std::string last_colon_part;
    
    std::istringstream iss(input_string);
    std::ostringstream oss;
    std::string line;

    while (std::getline(iss, line)) {    
        if (line.rfind(':') != std::string::npos) {
            last_colon_pos = line.rfind(':');
            last_colon_part = line.substr(0, last_colon_pos);
        }
        
        if (size_t last_pipe_pos = line.rfind('|'); last_pipe_pos != std::string::npos) {
            line.insert(last_pipe_pos, last_colon_part);
        }

        oss << line << '\n';
    }

    return std::regex_replace(oss.str(), std::regex(":|\\|"), "->");
}

#endif /* LRPARSER_HPP */