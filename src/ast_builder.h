#include <string>
#include <optional>
#include <stack>

#include "ast.h"

struct TreeNode {
    std::string name;
    std::optional<std::string> data;
    std::list<TreeNode> children;

    bool has_tokenized_child() const {
        return std::ranges::any_of(children, [](const TreeNode& child) { return child.data.has_value(); });
    }

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
            if (depth > 0) os << "|_ ";

            os << currentNode->name;
            if (currentNode->data.has_value()) { os << " '" << currentNode->data.value() << "'" << '\n'; } else { os << '\n'; }

            // Push children onto the stack in reverse order
            for (auto childIter = currentNode->children.rbegin(); childIter != currentNode->children.rend(); ++childIter) {
                nodeStack.emplace(std::to_address(childIter), depth + 1);
            }
        }

        return os;
    }
};

struct Symbol {
    std::variant<std::unique_ptr<BaseAST>, BabelType> value;

    explicit (false) Symbol(BabelType type) : value(type) {}
    explicit Symbol(std::unique_ptr<BaseAST> var) : value(std::move(var)) {}
    Symbol(Symbol&&) noexcept = default;
    Symbol &operator=(Symbol&&) noexcept = default;

    BabelType obtainType() {
        if (!std::holds_alternative<BabelType>(value))
            babel_panic("use of undeclared type '%s'", dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(value).get())->getName().c_str());

        return std::get<BabelType>(value);
    }
};

const std::unordered_map<std::string, BabelType> TypeMap = {
    {"Int", BabelType::Int()},
    {"Int8", BabelType::Int8()},
    {"Int16", BabelType::Int16()},
    {"Int32", BabelType::Int32()},
    {"Int64", BabelType::Int64()},
    {"Int128", BabelType::Int128()},
    {"Float", BabelType::Float()},
    {"Float16", BabelType::Float16()},
    {"Float32", BabelType::Float32()},
    {"Float64", BabelType::Float64()},
    {"Float128", BabelType::Float128()},
    {"Double", BabelType::Double()},
    {"Bool", BabelType::Boolean()},
    {"Char", BabelType::Character()},
    {"Cstr", BabelType::CString()},
    {"Void", BabelType::Void()},
};

Symbol getBabelType(std::stack<std::variant<TreeNode, std::unique_ptr<BaseAST>, Symbol>>& stack, int removeCount) {
    // ignore optionals, arrays and template stuff for now
    if (removeCount == 1) {
        std::string ty = std::get<TreeNode>(stack.top()).data.value(); stack.pop();
        
        if (TypeMap.contains(ty)) {
            return TypeMap.at(ty); 
        } else if (ClassTypes.contains(ty)) {
            return ClassTypes.at(ty);
        } else {
            // babel_panic("use of undeclared type '%s'", ty.c_str());
            // a bit hacky but well
            return Symbol{std::make_unique<VariableAST>(ty, std::nullopt, false, false, false)};
        }
    } else if (removeCount == 2) {
        BabelType type = std::get<Symbol>(stack.top()).obtainType(); stack.pop();

        if (std::get<TreeNode>(stack.top()).name == "MULTIPLY" || std::get<TreeNode>(stack.top()).name == "DEREF") {
            stack.pop();
            return BabelType::Pointer(xyz::indirect(type), false);
        }

        if (std::get<TreeNode>(stack.top()).name == "") {
            stack.pop();
            // question mark (optional type) in the future
            babel_stub();
        }
        
        babel_unreachable();
    } else if (removeCount == 3) {
        BabelType type = std::get<Symbol>(stack.top()).obtainType(); stack.pop();
        stack.pop(); stack.pop(); // *const
        return BabelType::Pointer(xyz::indirect(type), true);
    } else {
        size_t count = std::get<TreeNode>(stack.top()).data.value().size(); stack.pop();
        std::deque<std::variant<xyz::indirect<BabelType>, std::tuple<llvm::Constant*, xyz::indirect<BabelType>>>> templates = {};

        while (count-- > 0) {
            while (!std::holds_alternative<TreeNode>(stack.top())) {
                if (std::holds_alternative<Symbol>(stack.top())) {
                    Symbol sym = std::move(std::get<Symbol>(stack.top())); stack.pop();

                    if (std::holds_alternative<BabelType>(sym.value)) {
                        templates.emplace_front(xyz::indirect(sym.obtainType()));
                    } else {
                        // TODO: variable codegenComptime will return nullptr at this point
                        std::unique_ptr<BaseAST> astNode = std::move(std::get<std::unique_ptr<BaseAST>>(sym.value));
                        templates.emplace_front(std::tuple<llvm::Constant*, xyz::indirect<BabelType>>{astNode->codegenComptime(), xyz::indirect(astNode->getType())});
                    }
                } else {
                    std::unique_ptr<BaseAST> astNode = std::move(std::get<std::unique_ptr<BaseAST>>(stack.top())); stack.pop();
                    templates.emplace_front(std::tuple<llvm::Constant*, xyz::indirect<BabelType>>{astNode->codegenComptime(), xyz::indirect(astNode->getType())});
                }

                if (std::get<TreeNode>(stack.top()).name == "COMMA")
                    stack.pop();
            }
            stack.pop(); // LT

            std::string name = std::get<TreeNode>(stack.top()).data.value(); stack.pop();
            templates = {xyz::indirect(BabelType::Aggregate(name, templates))};
        }

        return *std::get<xyz::indirect<BabelType>>(templates.at(0));
    }
}

void buildNode(std::stack<std::variant<TreeNode, std::unique_ptr<BaseAST>, Symbol>>& nodeStack, std::string_view type, int removeCount) {
    std::variant<TreeNode, std::unique_ptr<BaseAST>, Symbol> node;

    if (type == "atom" || type == "literal") {
        TreeNode atom = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        if (atom.name == "INTEGER") {
            node = std::make_unique<IntegerAST>(atom.data.value());
        } else if (atom.name == "FLOATING_POINT") {
            node = std::make_unique<FloatingPointAST>(atom.data.value());
        } else if (atom.name == "BOOL") {
            node = std::make_unique<BooleanAST>(atom.data.value());
        } else if (atom.name == "IDENTIFIER") {
            node = std::make_unique<VariableAST>(atom.data.value(), std::nullopt, false, false, false);
        } else if (atom.name == "CSTRING") {
            node = std::make_unique<CStringAST>(unescapeString(atom.data.value().substr(2, atom.data.value().size() - 3)));
        } else if (atom.name == "STRING") {
            babel_stub();
        } else if (atom.name == "CHAR") {
            node = std::make_unique<CharacterAST>(atom.data.value().at(1));
        }

        //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        //fprintf(stderr, "\n");
    } else if (type == "type") {
        node = getBabelType(nodeStack, removeCount);
    } else if (type == "sum" || type == "term" || type == "exponentiation" || type == "shift_expression" || type == "bitwise_and" || type == "bitwise_or" || type == "bitwise_xor") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        std::unique_ptr<BaseAST> lhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

        node = std::make_unique<BinaryOperatorAST>(op.data.value(), std::move(lhs), std::move(rhs));
        
        //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        //fprintf(stderr, "\n");
    } else if (type == "comparison" || type == "conjunction" || type == "disjunction") {
        node = std::move(nodeStack.top()); nodeStack.pop();
        
        std::deque<std::string> ops = {};
        std::deque<std::unique_ptr<BaseAST>> vals = {};

        auto isComp = [&nodeStack, &type]() { return std::get<TreeNode>(nodeStack.top()).name == "cmp_op" && type == "comparison"; };
        auto isAnd = [&nodeStack, &type]() { return std::get<TreeNode>(nodeStack.top()).name == "AND" && type == "conjunction"; };
        auto isOr = [&nodeStack, &type]() { return std::get<TreeNode>(nodeStack.top()).name == "OR" && type == "disjunction"; };

        while (std::holds_alternative<TreeNode>(nodeStack.top()) && (isComp() || isAnd() || isOr())) {
            std::string op = std::get<TreeNode>(nodeStack.top()).name == "cmp_op"
                ? std::get<TreeNode>(nodeStack.top()).children.front().data.value()
                : std::get<TreeNode>(nodeStack.top()).data.value();

            ops.push_front(op);
            nodeStack.pop(); // operator
            vals.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()))); nodeStack.pop();
        }

        if (!ops.empty()) {
            vals.push_back(std::move(std::get<std::unique_ptr<BaseAST>>(node)));
            node = std::make_unique<ComparisonChainAST>(std::move(ops), std::move(vals));
        }
    } else if (type == "inversion" || type == "prefix") {
        std::unique_ptr<BaseAST> operand = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        std::string s = (op.name == "INCREMENT" || op.name == "DECREMENT") ? "pre" : "";

        if (op.data.value() == "&") {
            node = std::make_unique<AddressOfOperatorAST>(std::move(operand));
        } else {
            node = std::make_unique<UnaryOperatorAST>(s + op.data.value(), std::move(operand));
        }
    } else if (type == "postfix") {
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "RSQUARE") {
            nodeStack.pop();
            std::unique_ptr<BaseAST> index = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            nodeStack.pop(); // LSQUARE
            std::unique_ptr<BaseAST> container = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

            node = std::make_unique<AccessElementOperatorAST>(std::move(container), std::move(index));
        } else if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "DEREF") {
            nodeStack.pop();
            node = std::make_unique<DereferenceOperatorAST>(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()))); nodeStack.pop();
        } else {
            TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
            std::unique_ptr<BaseAST> operand = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

            node = std::make_unique<UnaryOperatorAST>("post" + op.data.value(), std::move(operand));
        }
    } else if (type == "primary") {
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "RPAREN") {
            nodeStack.pop();
            node = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            nodeStack.pop(); //LPAREN
        }
    } else if (type == "assignment") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        
        std::optional<BabelType> varType = std::nullopt;
        if (std::holds_alternative<Symbol>(nodeStack.top())) {
            varType = std::get<Symbol>(nodeStack.top()).obtainType(); nodeStack.pop();
            nodeStack.pop(); // COLON
        }

        TreeNode var = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        bool isDeclaration = false;
        bool isConstant = false;
        if (!nodeStack.empty() && std::holds_alternative<TreeNode>(nodeStack.top()) && (std::get<TreeNode>(nodeStack.top()).name == "LET" || std::get<TreeNode>(nodeStack.top()).name == "CONST")) {
            TreeNode vardecl = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
            isDeclaration = true;
            isConstant = vardecl.name == "CONST";
        }

        if (auto subop = op.children.front().data.value(); subop != "=") {
            auto subexpr = std::make_unique<BinaryOperatorAST>(subop.substr(0, subop.size() - 1), std::make_unique<VariableAST>(var.data.value(), std::nullopt, isConstant, isDeclaration, rhs->isComptimeAssignable()), std::move(rhs));
            node = std::make_unique<BinaryOperatorAST>("=", std::make_unique<VariableAST>(var.data.value(), std::nullopt, isConstant, isDeclaration, subexpr->isComptimeAssignable()), std::move(subexpr));
        } else {
            if (!varType.has_value()) varType = rhs->getType();
            node = std::make_unique<BinaryOperatorAST>(subop, std::make_unique<VariableAST>(var.data.value(), varType, isConstant, isDeclaration, rhs->isComptimeAssignable()), std::move(rhs));
        }
    } else if (type == "short_declaration") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); // COLON_EQUALS
        TreeNode var = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        node = std::make_unique<BinaryOperatorAST>(":=", std::make_unique<VariableAST>(var.data.value(), rhs->getType(), false, true, rhs->isComptimeAssignable()), std::move(rhs));        
    } else if (type == "element_assignment") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        nodeStack.pop(); // RSQUARE
        std::unique_ptr<BaseAST> index = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); // LSQUARE
        TreeNode var = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        if (auto subop = op.children.front().data.value(); subop != "=") {
            auto subexpr = std::make_unique<BinaryOperatorAST>(subop.substr(0, subop.size() - 1), std::make_unique<AccessElementOperatorAST>(std::make_unique<VariableAST>(var.data.value(), std::nullopt, false, false, false), std::move(index)), std::move(rhs));
            node = std::make_unique<BinaryOperatorAST>("=", std::make_unique<AccessElementOperatorAST>(std::make_unique<VariableAST>(var.data.value(), std::nullopt, false, false, false), std::move(index)), std::move(subexpr));
        } else {
            node = std::make_unique<BinaryOperatorAST>(subop, std::make_unique<AccessElementOperatorAST>(std::make_unique<VariableAST>(var.data.value(), std::nullopt, false, false, false), std::move(index)), std::move(rhs));
        }
    } else if (type == "indirect_assignment") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        TreeNode chained_deref = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        TreeNode var = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        auto makeDerefExpr = [&](void) {
            std::unique_ptr<BaseAST> expr =
                std::make_unique<VariableAST>(var.data.value(), std::nullopt, false, false, false);

            for (size_t i = 0; i < chained_deref.children.size(); ++i) {
                expr = std::make_unique<DereferenceOperatorAST>(std::move(expr));
            }

            return expr;
        };

        if (auto subop = op.children.front().data.value(); subop != "=") {
            auto subexpr = std::make_unique<BinaryOperatorAST>(subop.substr(0, subop.size() - 1), makeDerefExpr(), std::move(rhs));
            node = std::make_unique<BinaryOperatorAST>("=", makeDerefExpr(), std::move(subexpr));
        } else {
            node = std::make_unique<BinaryOperatorAST>(subop, makeDerefExpr(), std::move(rhs));
        }
    } else if (type == "if_stmt") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        if (std::get<TreeNode>(nodeStack.top()).name == "ELSE") {
            nodeStack.pop(); // ELSE
            
            std::deque<std::unique_ptr<BaseAST>> elif_statements;
            while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
                elif_statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
                nodeStack.pop();
            }
            auto elif_block = std::make_unique<BlockAST>(std::move(elif_statements));
            nodeStack.pop(); // THEN

            auto condition = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            block = std::make_unique<IfStmtAST>(std::move(condition), std::move(elif_block), std::move(block));

            bool is_if = std::get<TreeNode>(nodeStack.top()).name == "IF";
            nodeStack.pop(); // ELIF or IF

            if (is_if) {
                node = std::move(block);
                goto done;
            }

        } else {
            nodeStack.pop(); // THEN

            auto condition = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            block = std::make_unique<IfStmtAST>(std::move(condition), std::move(block), nullptr);

            bool is_if = std::get<TreeNode>(nodeStack.top()).name == "IF";
            nodeStack.pop(); // ELIF or IF

            if (is_if) {
                node = std::move(block);
                goto done;
            }
        }

        while (true) {
            std::deque<std::unique_ptr<BaseAST>> elif_statements;
            while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
                elif_statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
                nodeStack.pop();
            }
            auto elif_block = std::make_unique<BlockAST>(std::move(elif_statements));
            nodeStack.pop(); // THEN

            auto condition = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            block = std::make_unique<IfStmtAST>(std::move(condition), std::move(elif_block), std::move(block));

            bool is_if = std::get<TreeNode>(nodeStack.top()).name == "IF";
            nodeStack.pop(); // ELIF or IF

            if (is_if) {
                node = std::move(block);
                goto done;
            }
        }

        done:
            //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
            fprintf(stderr, "\n");
    } else if (type == "elif_stmt" || type == "args" || type == "params" || type == "generic_list" || type == "macro_params" || type == "type_spec" || type == "type_signature" || type == "and_chain" || type == "or_chain") {
        return; // handled by it's corresponding statement
    } else if (type == "while_loop") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        nodeStack.pop(); // DO
        std::unique_ptr<BaseAST> cond = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); // WHILE

        std::optional<std::string> label = std::nullopt;
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "COLON") {
            nodeStack.pop(); // COLON
            label = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
            nodeStack.pop(); // LABEL_START
        }

        node = std::make_unique<WhileLoopAST>(label, std::move(cond), std::move(block));
    } else if (type == "for_loop") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        nodeStack.pop(); // DO

        std::unique_ptr<BaseAST> inc = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); // SEMICOLON
        std::unique_ptr<BaseAST> cond = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); // SEMICOLON
        std::unique_ptr<BaseAST> init = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

        nodeStack.pop(); // FOR

        std::optional<std::string> label = std::nullopt;
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "COLON") {
            nodeStack.pop(); // COLON
            label = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
            nodeStack.pop(); // LABEL_START
        }

        node = std::make_unique<ForLoopAST>(label, std::move(init), std::move(cond), std::move(inc), std::move(block));
    } else if (type == "for_in_loop") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        nodeStack.pop(); // DO

        std::unique_ptr<BaseAST> collection = std::make_unique<VariableAST>(std::get<TreeNode>(nodeStack.top()).data.value(), std::nullopt, false, false, false); nodeStack.pop();
        nodeStack.pop(); // IN
        std::unique_ptr<BaseAST> elmntName = std::make_unique<VariableAST>(std::get<TreeNode>(nodeStack.top()).data.value(), std::nullopt, false, false, false); nodeStack.pop();
        nodeStack.pop(); // FOR

        std::optional<std::string> label = std::nullopt;
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "COLON") {
            nodeStack.pop(); // COLON
            label = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
            nodeStack.pop(); // LABEL_START          
        }

        node = std::make_unique<ForInLoopAST>(label, std::move(elmntName), std::move(collection), std::move(block));
    } else if (type == "continue_stmt") {
        std::optional<std::string> label = std::nullopt;
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "IDENTIFIER") {
            label = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        }

        nodeStack.pop(); // CONTINUE
        node = std::make_unique<ContinueStmtAST>(label);
    } else if (type == "break_stmt") {
        std::optional<std::string> label = std::nullopt;
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "IDENTIFIER") {
            label = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        }

        nodeStack.pop(); // BREAK
        node = std::make_unique<BreakStmtAST>(label);
    } else if (type == "return_stmt") {
        // assuming not multiple return values
        if (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            node = std::make_unique<ReturnStmtAST>(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
            nodeStack.pop(); // RETURN
        } else {
            node = std::make_unique<ReturnStmtAST>(nullptr);
            nodeStack.pop(); // RETURN
        }
    } else if (type == "goto_stmt") {
        std::string target = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // GOTO

        node = std::make_unique<GotoStmtAST>(target);
    } else if (type == "label_stmt") {
        std::string name = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // LABEL_START

        node = std::make_unique<LabelStmtAST>(name);
    } else if (type == "extern_task") {
        BabelType retType = std::get<Symbol>(nodeStack.top()).obtainType(); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // RARR and RPAREN

        std::deque<BabelType> ArgTypes;
        bool isVarArg = false;

        bool isFirstIter = true;
        while (!std::holds_alternative<TreeNode>(nodeStack.top()) || std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "VARARG") {
                isVarArg = true;
                nodeStack.pop(); // VARARG

                if (!isFirstIter)
                    babel_panic("variable arguments must appear last in task definition");
            } else {
                ArgTypes.push_front(std::get<Symbol>(nodeStack.top()).obtainType()); nodeStack.pop();
            }

            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();

            isFirstIter = false;
        }

        nodeStack.pop(); // LPAREN
        std::string TaskName = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // TASK and EXTERN

        node = std::make_unique<TaskHeaderAST>(TaskName, std::deque<std::string>(ArgTypes.size(), "") , ArgTypes, retType, isVarArg);
    } else if (type == "task_header") {
        BabelType retType = std::get<Symbol>(nodeStack.top()).obtainType(); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // RARR and RPAREN

        std::deque<std::string> ArgNames;
        std::deque<BabelType> ArgTypes;
        bool isVarArg = false;

        bool isFirstIter = true;
        while (!std::holds_alternative<TreeNode>(nodeStack.top()) || std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "VARARG") {
                isVarArg = true;
                nodeStack.pop(); // VARARG

                if (!isFirstIter)
                    babel_panic("variable arguments must appear last in task definition");
            } else {
                // assuming no default value for now
                ArgTypes.push_front(std::get<Symbol>(nodeStack.top()).obtainType()); nodeStack.pop();
                nodeStack.pop(); // COLON
                ArgNames.push_front(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();
            }

            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();

            isFirstIter = false;
        }

        nodeStack.pop(); // LPAREN
        std::string TaskName = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // TASK
        
        node = std::make_unique<TaskHeaderAST>(TaskName, std::move(ArgNames), std::move(ArgTypes), retType, isVarArg);
    } else if (type == "task_def") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        nodeStack.pop(); // DO

        auto ptr = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        auto header = dynamic_unique_cast<TaskHeaderAST>(std::move(ptr));
        node = std::make_unique<TaskAST>(std::move(header), std::move(block));
    } else if (type == "macro_call") {
        std::deque<std::variant<std::unique_ptr<BaseAST>, BabelType>> Args;
        if (std::get<TreeNode>(nodeStack.top()).name == "RPAREN") {
            nodeStack.pop(); // RPAREN

            // assuming expressions/types as params
            while (!std::holds_alternative<TreeNode>(nodeStack.top()) || std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
                if (std::holds_alternative<Symbol>(nodeStack.top())) {
                    Args.emplace_front(std::move(std::get<Symbol>(nodeStack.top()).value)); nodeStack.pop();
                } else if (dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()).get())) {
                    std::string s = dynamic_cast<VariableAST*>(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()).get())->getName(); nodeStack.pop();
                    nodeStack.emplace(TreeNode{"type", s, {}});
                    nodeStack.emplace(getBabelType(nodeStack, 1));
                    continue;
                } else {
                    Args.emplace_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()))); nodeStack.pop();
                }

                if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                    nodeStack.pop();
            }
            nodeStack.pop(); // LPAREN
        }

        auto name = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // AT
        node = std::make_unique<MacroCallAST>(name, std::move(Args));
    } else if (type == "function_call") {
        nodeStack.pop(); // RPAREN

        // assuming expressions as params
        std::deque<std::unique_ptr<BaseAST>> Args;
        while (!std::holds_alternative<TreeNode>(nodeStack.top()) || std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            Args.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()))); nodeStack.pop();
            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();
        }

        nodeStack.pop(); // LPAREN
        auto name = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        node = std::make_unique<TaskCallAST>(name, std::move(Args));
    } else if (type == "class_construction") {
        nodeStack.pop(); // RPAREN

        std::deque<std::unique_ptr<BaseAST>> Args;
        while (!std::holds_alternative<TreeNode>(nodeStack.top()) || std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            Args.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top()))); nodeStack.pop();
            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();
        }

        nodeStack.pop(); // LPAREN
        auto name = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // NEW

        if (name == "Array") {
            node = std::make_unique<ArrayAST>(std::move(Args));
        } else {
            babel_stub();
        }
    } else if (type == "simple_stmt") {
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "NOOP") {
            nodeStack.pop(); // NOOP
            node = std::make_unique<BlockAST>(std::deque<std::unique_ptr<BaseAST>>{});
        } else {
            node = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            if (!std::get<std::unique_ptr<BaseAST>>(node)->isStatementLike())
                babel_panic("expression has no effect as a statement");
        }
    } else if (type == "terminator") {
        nodeStack.pop();
        return;
    } else {
        node = TreeNode();
        std::get<TreeNode>(node).name = type;
        for (int i = 0; i < removeCount; i++) {
            if (!std::holds_alternative<TreeNode>(nodeStack.top()))
                continue;

            TreeNode rdChild = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
            if (rdChild.children.empty() && !rdChild.data.has_value())
                continue;

            std::get<TreeNode>(node).children.push_front(rdChild);
        }
    }

    nodeStack.push(std::move(node));
}
