#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/string.hpp>
#include <cassert>
#include <fstream>
#include <list>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <variant>
#include <vector>
#include "tools.h"
#include "lexer.h"

#include <iostream>

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

template<typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<K, V>& myMap) {
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
        error_log = file + ": SyntaxError: " + msg + " at line " + std::to_string(line);
    }

    const char* what() const noexcept override {
        return error_log.c_str();
    }
};

// Define a macro to create the exception with file and line
#define SyntaxError(msg) throw SyntaxError(msg, __FILE__, __LINE__)

using standalone_function_t = void (*)();

const std::string EPSILON = "''";

class Rule;

class Grammar {
    private:
        void initializeRulesAndAlphabetAndNonterminals (const std::string& text);

        void initializeAlphabetAndTerminals ();

        bool collectDevelopmentFirsts(const std::list<std::string>& development, std::list<std::string>& nonterminalFirsts);

        void initializeFirsts ();

        void initializeFollows();

    public:
        std::list<std::string> alphabet;
        std::list<std::string> nonterminals;
        std::list<std::string> terminals;
        std::list<Rule> rules;
        std::string text;
        mutable std::map<std::string, std::list<std::string>> firsts;
        std::map<std::string, std::list<std::string>> follows;
        std::string axiom;

        Grammar() = default;
        explicit Grammar(std::string const& text) {
            initializeRulesAndAlphabetAndNonterminals(text);
            initializeAlphabetAndTerminals();
            initializeFirsts();
            initializeFollows();
        }

        std::list<Rule> getRulesForNonterminal(const std::string& nonterminal) const;

        std::list<std::string> getSequenceFirsts(const std::list<std::string>& sequence) const {
            std::list<std::string> result = {};
            bool epsilonInSymbolFirsts = true;

            for (const std::string& symbol : sequence) { //potentially mark const
                epsilonInSymbolFirsts = false;

                if (std::find(terminals.begin(), terminals.end(), symbol) != terminals.end()) {
                    addUnique(symbol, result);

                    break;
                }
                
                for (const std::string& first : firsts.at(symbol)) { //potentially mark const
                    epsilonInSymbolFirsts |= first == EPSILON;
                    addUnique(first, result);
                }
                
                epsilonInSymbolFirsts |= firsts.at(symbol).size() == 0;

                if (!epsilonInSymbolFirsts) break;                
            }

            if (epsilonInSymbolFirsts) addUnique(EPSILON, result);

            return result;
        }

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & alphabet;
            ar & nonterminals;
            ar & terminals;
            ar & rules;
            ar & text;
            ar & firsts;
            ar & follows;
            ar & axiom;
        }        
};

class Rule {
    public:
        const Grammar* grammar;
        int index;
        std::string nonterminal;
        std::list<std::string> pattern;
        std::list<std::string> development;
        
        Rule() = default;
        Rule(const Grammar* grammar, const std::string& text) : grammar(grammar), index(static_cast<int>(grammar->rules.size()) ) {
            std::list<std::string> split = splitString(text, "->");
            nonterminal = boost::trim_copy(split.front());
            pattern = trimElements(splitString(nonterminal, " "));
            development = trimElements(splitString(boost::trim_copy(split.back()), " "));
        }
        
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

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & grammar;
            ar & index;
            ar & nonterminal;
            ar & pattern;
            ar & development;
        }
};

void Grammar::initializeRulesAndAlphabetAndNonterminals (const std::string& text) {
    std::list<std::string> lines = splitString(text, "\n");

    for (const std::string& _ : lines) { //potentially mark const
        std::string line = boost::trim_copy(_);

        if (line != "") {
            // should use unique_pointr here later
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
    for (const Rule& rule : rules) { //potentially mark const
        for (const std::string& symbol : rule.development) { //potentially mark const
            if (symbol != EPSILON && symbol != "$" && std::find(nonterminals.begin(), nonterminals.end(), symbol) == nonterminals.end()) {
                addUnique(symbol, alphabet);
                addUnique(symbol, terminals);
            }
        }
    }
}

bool Grammar::collectDevelopmentFirsts(const std::list<std::string>& development, std::list<std::string>& nonterminalFirsts) {
    bool result = false;
    bool epsilonInSymbolFirsts = true;
    
    for (const std::string& symbol : development) { //potentially mark const
        epsilonInSymbolFirsts = false;

        if (isElement(symbol, terminals)) {
            result |= addUnique(symbol, nonterminalFirsts);
            
            break;
        }

        if (firsts.find(symbol) != firsts.end()) {
            for (const std::string& first : firsts.at(symbol)) { //potentially mark const
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
                std::string symbol = getElement(i, rule.development);

                if (isElement(symbol, nonterminals)) {
                    std::list<std::string> symbolFollows = getOrCreateArray(follows, symbol);
                    
                    std::list<std::string> sliced = rule.development;
                    auto end = sliced.begin();
                    std::advance(end, i + 1);
                    sliced.erase(sliced.begin(), end);
                    
                    std::list<std::string> afterSymbolFirsts = getSequenceFirsts(sliced);

                    for (const std::string& first : afterSymbolFirsts) { //potentially mark const
                        if (first == EPSILON) {
                            std::list<std::string> nonterminalFollows = follows[rule.nonterminal];

                            for (const std::string& _ : nonterminalFollows) { //potentially mark const
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

std::list<Rule> Grammar::getRulesForNonterminal(const std::string& nonterminal) const {
    std::list<Rule> result = {};

    for (Rule rule : rules) {
        if (nonterminal == rule.nonterminal) {
            result.push_back(rule);
        }
    }
    
    return result;
}

class UnifiedItem {
    public:
        Rule rule;
        int dotIndex;
        std::list<std::string> lookAheads = {};

        UnifiedItem(const Rule& rule, int dotIndex) : rule(rule), dotIndex(dotIndex) {
            if (rule.index == 0) {
                lookAheads.emplace_back("$");
            }
        }

        std::list<UnifiedItem> newItemsFromSymbolAfterDot() const {
            std::list<UnifiedItem> result = {};
            //std::string ntR = getElement(dotIndex, rule.development);
            auto it = std::next(rule.development.begin(), dotIndex);
            
            std::list<Rule> nonterminalRules = {};
            if (it != rule.development.end()) {
                nonterminalRules = rule.grammar->getRulesForNonterminal(*it);
            }

            for (Rule& ntRule : nonterminalRules) {
                addUnique(UnifiedItem(ntRule, 0) , result);
            }

            if (result.empty()) return result;

            std::list<std::string> newLookAheads = {};
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
                for (const std::string& _ : lookAheads) {
                    addUnique(_, newLookAheads);
                }
            }

            for (UnifiedItem& item : result) {
                item.lookAheads = slice(newLookAheads, 0);
            }
            
            return result;
        }

        std::optional<UnifiedItem> newItemAfterShift() {
            std::optional<UnifiedItem> result;
            
            if (dotIndex < rule.development.size() && getElement(dotIndex, rule.development) != EPSILON) {
                result.emplace(UnifiedItem(rule, dotIndex + 1));
            } else {
                return std::nullopt;
            }            
            
            result.value().lookAheads = slice(lookAheads, 0);            

            return result;
        }

        bool addUniqueTo(std::list<UnifiedItem>& items) const {
            bool result = false;

            for (UnifiedItem& item : items) {
                if (superEquals(item)) {
                    for (const std::string& _ : lookAheads) {
                        result |= addUnique(_, item.lookAheads);
                    }

                    return result;
                }
            }

            items.push_back(*this);

            return true;
        }

        bool superEquals(const UnifiedItem& that) const {
            return rule == that.rule && dotIndex == that.dotIndex;
        }

        bool operator==(const UnifiedItem& that) const {
            return superEquals(that) && includeEachOther(lookAheads, that.lookAheads);
        }
};

class Kernel {
    public:
        int index;
        std::list<UnifiedItem> items;
        std::list<UnifiedItem> closure;
        std::map<std::string, int> gotos;
        std::list<std::string> keys;
        
        //maybe initialize with grammar
        Kernel (int index, const std::list<UnifiedItem>& items) : index(index), items(items), closure(slice(items, 0)) {}

        bool operator==(const Kernel& that) const {
            return includeEachOther(items, that.items);
        }
};

class LRClosureTable {
    public:
        Grammar& grammar;
        std::list<Kernel> kernels;

        explicit LRClosureTable(Grammar& grammar) : grammar(grammar) {
            std::list<UnifiedItem> l = {UnifiedItem(grammar.rules.front(), 0)};
            
            Kernel k(0, l);
            kernels.push_back(k);

            //for (auto it = kernels.begin(); it != kernels.end();) {
            for (int i = 0; i < kernels.size();) {
                //Kernel& kernel = *it;
                auto it = std::next(kernels.begin(), i);
                Kernel& kernel = *it;
                
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
                auto closure = *it;
                std::list<UnifiedItem> newItemsFromSymbolAfterDot = closure.newItemsFromSymbolAfterDot();

                for (const UnifiedItem& item : newItemsFromSymbolAfterDot) {
                    item.addUniqueTo(kernel.closure);
                }
            }
        }

        bool addGotos(Kernel& kernel, std::list<Kernel>& kernel_list) const {
            bool lookAheadsPropagated = false;
            std::map<std::string, std::list<UnifiedItem>> newKernels;

            for (UnifiedItem& item : kernel.closure) {
                std::optional<UnifiedItem> newItem = item.newItemAfterShift();

                if (newItem != std::nullopt) {
                    std::string symbolAfterDot = getElement(item.dotIndex, item.rule.development);

                    addUnique(symbolAfterDot, kernel.keys);
                    getOrCreateArray(newKernels, symbolAfterDot);
                    std::list<UnifiedItem>& newItems = newKernels.at(symbolAfterDot);
                    newItem.value().addUniqueTo(newItems);
                }
            }
            
            for (const std::string& key : kernel.keys) {
                Kernel newKernel(static_cast<int>(kernels.size()), newKernels.at(key));
                int targetKernelIndex = indexOf(newKernel, kernels);

                if (targetKernelIndex < 0) {
                    kernel_list.push_back(newKernel);
                    targetKernelIndex = newKernel.index;
                } else {
                    for (const UnifiedItem& item : newKernel.items) {
                        std::list<UnifiedItem>& items = getUpdateableElement(targetKernelIndex, kernel_list).items;
                        lookAheadsPropagated |= item.addUniqueTo(items);
                    }
                }
                
                kernel.gotos.insert({key, targetKernelIndex});
            }
            
            return lookAheadsPropagated;
        }
};

class LRAction {
    public:
        std::string actionType; // could be optimized to char
        int actionValue;

        LRAction() = default;
        LRAction(std::string const& actionType, int actionValue) : actionType(actionType), actionValue(actionValue) {}

        std::string toString() const {
            return actionType + std::to_string(actionValue);
        }

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & actionType;
            ar & actionValue;
        }

        friend std::ostream& operator<<(std::ostream& os, const LRAction& lrAction) {
            os << lrAction.toString();
            return os;
        }
};

class State {
    public:
        int index;
        std::map<std::string, LRAction> mapping;

        State() = default;
        explicit State(std::list<State> const& states) : index(static_cast<int>(states.size())) {}

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & index;
            ar & mapping;
        }

        friend std::ostream& operator<<(std::ostream& os, const State& state) {
            os << state.index << ": " << state.mapping;
            return os;
        }
        
};

class LRTable {
    public:
        Grammar grammar;
        std::list<State> states = {};

        LRTable() = default;
        explicit LRTable(const LRClosureTable& closureTable) : grammar(closureTable.grammar) {
            for (const Kernel& kernel : closureTable.kernels) {
                State state(states);

                for (const std::string& key : kernel.keys) {                    
                    int nextStateIndex = kernel.gotos.at(key);
                    state.mapping.insert({key, LRAction((isElement(key, closureTable.grammar.terminals) ? "s" : ""), nextStateIndex)});
                }

                for (const UnifiedItem& item : kernel.closure) {
                    if (item.dotIndex == item.rule.development.size() || item.rule.development.front() == EPSILON) {
                        for (const std::string& lookAhead : item.lookAheads) {
                            state.mapping.insert({lookAhead, LRAction("r", item.rule.index)});
                        }
                    }
                }
                
                states.push_back(state);
            }
        }

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & grammar;
            ar & states;
        }
};

std::optional<LRAction> chooseActionElement(State& state, const std::string& token) {
    if (state.mapping.find(token) == state.mapping.end()) {
        return std::nullopt;
    }

    return state.mapping.at(token);
}

struct TreeNode {
    std::string name;
    std::optional<std::string> data;
    std::list<TreeNode> children;

    friend std::ostream& operator<<(std::ostream& os, const TreeNode& node) {
        std::stack<std::pair<const TreeNode*, int>> nodeStack;
        nodeStack.push(std::make_pair(&node, 0));

        while (!nodeStack.empty()) {
            const TreeNode* currentNode = nodeStack.top().first;
            int depth = nodeStack.top().second;
            nodeStack.pop();

            // Print the current node
            for (int i = 0; i < depth; ++i) {
                os << "  ";
            }
            if(depth > 0) os << "|_ ";
            os << currentNode->name << std::endl;

            // Push children onto the stack in reverse order
            for (auto childIter = currentNode->children.rbegin(); childIter != currentNode->children.rend(); ++childIter) {
                nodeStack.push(std::make_pair(&(*childIter), depth + 1));
            }
        }

        return os;
    }
};

class Parser {
    public:
        LRTable lrTable;
        Parser() = default;
        explicit Parser(const LRTable& lrTable) : lrTable(lrTable) {}

        std::string retrieveMessage(const State& state, const std::string& token) const {
            std::map<std::string, LRAction> m = state.mapping;
            
            std::list<std::string> expected;
            for (auto it = m.begin(); it != m.end(); ++it) {
                if (isElement(it->first, lrTable.grammar.nonterminals)) {
                    expected.splice(expected.end(), lrTable.grammar.firsts.at(it->first));
                } else {
                    expected.push_back(it->first);
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

        std::variant<TreeNode, std::string> parse(std::list<Token> tokens) const {
            tokens.push_back(Token("$", "$"));
            std::stack<TreeNode> nodeStack;
            std::stack<int> stateStack;
            stateStack.push(0);
            int tokenIndex = 0;
            Token token = *std::next(tokens.begin(), tokenIndex);
            std::string token_type = token.getType();
            State state = *std::next(lrTable.states.begin(), stateStack.top());
            std::optional<LRAction> actionElement = chooseActionElement(state, token_type);

            while (actionElement != std::nullopt && actionElement.value().toString() != "r0") {
                if (actionElement.value().actionType == "s") {
                    nodeStack.push(TreeNode{(*std::next(tokens.begin(), tokenIndex)).getType(), std::optional((*std::next(tokens.begin(), tokenIndex)).getValue())});
                    stateStack.push(actionElement.value().actionValue);
                    tokenIndex++;
                } else if (actionElement.value().actionType == "r") {
                    int ruleIndex = actionElement.value().actionValue;
                    Rule rule = *std::next(lrTable.grammar.rules.begin(), ruleIndex);
                    int removeCount = isElement(EPSILON, rule.development) ? 0 : rule.development.size();

                    TreeNode newNode;
                    newNode.name = rule.nonterminal;

                    for (int i = 0; i < removeCount; i++) {
                        newNode.children.push_front(nodeStack.top());
                        nodeStack.pop();
                        stateStack.pop();
                    }

                    nodeStack.push(newNode);
                } else {
                    stateStack.push(actionElement.value().actionValue);
                }
                
                state = *std::next(lrTable.states.begin(), stateStack.top());
                token_type = (nodeStack.size() + stateStack.size()) % 2 == 0 ? nodeStack.top().name : (*std::next(tokens.begin(), tokenIndex)).getType();
                actionElement = chooseActionElement(state, token_type);
            }

            if (actionElement != std::nullopt && actionElement.value().toString() == "r0") {
                return TreeNode{lrTable.grammar.axiom, std::nullopt, {nodeStack.top()}};
            }

            return "SyntaxError: " + retrieveMessage(state, (*std::next(tokens.begin(), tokenIndex)).getValue());
        }

        template <class Archive>
        void serialize(Archive& ar, const unsigned int /* version */) {
            ar & lrTable;
        }
};

std::string transform_string(const std::string& input_string) {
    std::istringstream iss(input_string);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    size_t last_colon_pos;
    std::string last_colon_part;

    for (auto& line : lines) {
        
        if (line.rfind(':') != std::string::npos) {
            last_colon_pos = line.rfind(':');
            last_colon_part = line.substr(0, last_colon_pos);
        }
        
        size_t last_pipe_pos = line.rfind('|');
        if (last_pipe_pos != std::string::npos) {
            line.insert(last_pipe_pos, last_colon_part);
        }
    }

    std::ostringstream oss;
    for (const auto& line : lines) {
        oss << line << '\n';
    }
    
    return std::regex_replace(oss.str(), std::regex(":|\\|"), "->");
}
