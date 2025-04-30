#include <string>
#include <optional>
#include <stack>

#include "ast.h"

struct TreeNode {
    std::string name;
    std::optional<std::string> data;
    std::deque<TreeNode> children;

    TreeNode() = default;

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

void buildNode(std::stack<std::variant<TreeNode, std::unique_ptr<BaseAST>>>& nodeStack, std::string_view type, int removeCount) {
    std::variant<TreeNode, std::unique_ptr<BaseAST>> node;

    if (type == "atom") {
        TreeNode atom = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        if (atom.name == "INTEGER") {
            node = std::make_unique<IntegerAST>(std::stoi(atom.data.value()));

            std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
            fprintf(stderr, "\n");
        } else if (atom.name == "FLOATING_POINT") {
            node = std::make_unique<FloatingPointAST>(std::stod(atom.data.value()));

            std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
            fprintf(stderr, "\n");
        }
    } else if (type == "sum" || type == "term") {
        std::unique_ptr<BaseAST> rhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();
        TreeNode op = std::get<TreeNode>(nodeStack.top()); nodeStack.pop();
        std::unique_ptr<BaseAST> lhs = std::move(std::get<std::unique_ptr<BaseAST>>(nodeStack.top())); nodeStack.pop();

        node = std::make_unique<BinaryOperatorAST>(op.data.value(), std::move(lhs), std::move(rhs));
        
        std::get<std::unique_ptr<BaseAST>>(node)->codegen()->print(llvm::errs());
        fprintf(stderr, "\n");
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
