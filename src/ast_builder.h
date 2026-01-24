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

BabelType getBabelType(TreeNode node) {
    // ignore optionals, arrays and template stuff for now

    const std::unordered_map<std::string, BabelType> TypeMap = {
        {"int", BabelType::Int()},
        {"int8", BabelType::Int8()},
        {"int16", BabelType::Int16()},
        {"int32", BabelType::Int32()},
        {"int64", BabelType::Int64()},
        {"int128", BabelType::Int128()},
        {"float", BabelType::Float()},
        {"float16", BabelType::Float16()},
        {"float32", BabelType::Float32()},
        {"float64", BabelType::Float64()},
        {"float128", BabelType::Float128()},
        {"bool", BabelType::Boolean()},
        {"char", BabelType::Character()},
        {"cstr", BabelType::CString()},
        {"void", BabelType::Void()},
    };

    return TypeMap.at(node.data.value());
}

void buildNode(std::stack<std::variant<TreeNode, std::unique_ptr<BaseAST>>>& nodeStack, std::string_view type, int removeCount) {
    std::variant<TreeNode, std::unique_ptr<BaseAST>> node;

    if (type == "atom") {
        TreeNode atom = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        if (atom.name == "INTEGER") {
            node = std::make_unique<IntegerAST>(std::stoi(atom.data.value()));
        } else if (atom.name == "FLOATING_POINT") {
            node = std::make_unique<FloatingPointAST>(std::stod(atom.data.value()));
        } else if (atom.name == "BOOL") {
            node = std::make_unique<BooleanAST>(atom.data.value());
        } else if (atom.name == "VAR") {
            node = std::make_unique<VariableAST>(atom.data.value(), std::nullopt, false, false, false);
        } else if (atom.name == "STRING") {
            node = std::make_unique<CStringAST>(unescapeString(atom.data.value().substr(1, atom.data.value().size() - 2)));
        }

        //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        //fprintf(stderr, "\n");
    } else if (type == "sum" || type == "term" || type == "shift_expression" || type == "bitwise_and" || type == "bitwise_or" || type == "bitwise_xor" || type == "comparison" || type == "conjunction" || type == "disjunction" || type == "contravalence") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        std::unique_ptr<BaseAST> lhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

        node = std::make_unique<BinaryOperatorAST>(op.data.value(), std::move(lhs), std::move(rhs));
        
        //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        //fprintf(stderr, "\n");
    } else if (type == "primary") {
        if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "RPAREN") {
            nodeStack.pop();
            node = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            nodeStack.pop(); //LPAREN
        } else if (std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "RSQUARE") {
            nodeStack.pop();
            std::unique_ptr<BaseAST> index = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            nodeStack.pop(); // LSQUARE
            std::unique_ptr<BaseAST> container = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

            node = std::make_unique<AccessElementOperatorAST>(std::move(container), std::move(index));
        }
    } else if (type == "inversion" || type == "factor") {
        std::unique_ptr<BaseAST> operand;
        TreeNode op;

        if (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            operand = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
            op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        } else {
            op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
            operand = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        }

        node = std::make_unique<UnaryOperatorAST>(op.data.value(), std::move(operand));

        std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        fprintf(stderr, "\n");
    } else if (type == "assignment") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        
        std::optional<BabelType> varType = std::nullopt;
        // TODO: Handle the new typing system! (also in the task headers/externs)
        if (std::get<TreeNode>(nodeStack.top()).name == "TYPE") {
            // varType = getBabelType(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();
            varType = getBabelType(std::get<TreeNode>(nodeStack.top())); nodeStack.pop();

            nodeStack.pop(); // COLON
        }

        TreeNode var = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();

        bool isDeclaration = false;
        bool isConstant = false;
        if (!nodeStack.empty() && std::holds_alternative<TreeNode>(nodeStack.top()) && std::get<TreeNode>(nodeStack.top()).name == "VARDECL") {
            TreeNode vardecl = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
            isDeclaration = true;
            isConstant = vardecl.data.value() == "const";
        }

        if (auto subop = op.children.front().data.value(); subop != "=") {
            auto subexpr = std::make_unique<BinaryOperatorAST>(subop.substr(0, subop.size() - 1), std::make_unique<VariableAST>(var.data.value(), std::nullopt, isConstant, isDeclaration, rhs->isComptimeAssignable()), std::move(rhs));
            node = std::make_unique<BinaryOperatorAST>("=", std::make_unique<VariableAST>(var.data.value(), std::nullopt, isConstant, isDeclaration, subexpr->isComptimeAssignable()), std::move(subexpr));
        } else {
            if (!varType.has_value()) varType = rhs->getType();
            node = std::make_unique<BinaryOperatorAST>(subop, std::make_unique<VariableAST>(var.data.value(), varType, isConstant, isDeclaration, rhs->isComptimeAssignable()), std::move(rhs));
        }
        
        //std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        //fprintf(stderr, "\n");
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
    } else if (type == "elif_stmt" || type == "task_header" || type == "args" || type == "params" || type == "generic_list" || type == "type" || type == "type_spec") {
        return; // handled by it's corresponding statement
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
        // BabelType retType = getBabelType(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();
        BabelType retType = getBabelType(std::get<TreeNode>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // RARR and RPAREN

        std::deque<std::string> ArgNames;
        std::deque<BabelType> ArgTypes;
        while (std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            // ArgTypes.push_front(getBabelType(std::get<TreeNode>(nodeStack.top()).data.value())); nodeStack.pop();
            ArgTypes.push_front(getBabelType(std::get<TreeNode>(nodeStack.top()))); nodeStack.pop();
            nodeStack.pop(); // COLON
            ArgNames.push_front(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();

            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();
        }

        nodeStack.pop(); // LPAREN
        std::string TaskName = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // TASK and EXTERN

        node = std::make_unique<TaskHeaderAST>(TaskName, ArgNames, ArgTypes, retType);
    } else if (type == "task_def") {
        nodeStack.pop(); // END

        std::deque<std::unique_ptr<BaseAST>> statements;
        while (!std::holds_alternative<TreeNode>(nodeStack.top())) {
            statements.push_front(std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())));
            nodeStack.pop();
        }
        std::unique_ptr<BaseAST> block = std::make_unique<BlockAST>(std::move(statements));

        nodeStack.pop(); // DO
        // BabelType retType = getBabelType(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();
        BabelType retType = getBabelType(std::get<TreeNode>(nodeStack.top())); nodeStack.pop();
        nodeStack.pop(); nodeStack.pop(); // RARR and RPAREN

        std::deque<std::string> ArgNames;
        std::deque<BabelType> ArgTypes;
        while (std::get<TreeNode>(nodeStack.top()).name != "LPAREN") {
            // assuming no default value for now
            // ArgTypes.push_front(getBabelType(std::get<TreeNode>(nodeStack.top()).data.value())); nodeStack.pop();
            ArgTypes.push_front(getBabelType(std::get<TreeNode>(nodeStack.top()))); nodeStack.pop();
            nodeStack.pop(); // COLON
            ArgNames.push_front(std::get<TreeNode>(nodeStack.top()).data.value()); nodeStack.pop();

            if (std::get<TreeNode>(nodeStack.top()).name == "COMMA")
                nodeStack.pop();
        }

        nodeStack.pop(); // LPAREN
        std::string TaskName = std::get<TreeNode>(nodeStack.top()).data.value(); nodeStack.pop();
        nodeStack.pop(); // TASK
        
        auto header = std::make_unique<TaskHeaderAST>(TaskName, std::move(ArgNames), std::move(ArgTypes), retType);
        node = std::make_unique<TaskAST>(std::move(header), std::move(block));
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
