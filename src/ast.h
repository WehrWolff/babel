#include <memory>
#include <string>
#include <vector>

// Base class for all expression node
class BaseAST {
    public:
        virtual ~AST() = default;
};

// class for referencing variables
class VariableAST : public BaseAST {
    const std::string name;

    public:
        VariableAST (const std::string name&) : name(name) {}
};

// class for numeric literals which are integers
class IntegerAST : public BaseAST {
    const int value;

    public:
        IntegerAST (int value) : value(value) {}
};

// class for numeric literals which are floating points
class FloatingPointAST : public BaseAST {
    const double value;

    public:
        FloatingPointAST (double value) : value(value) {}
};

// class for when binary operators are used
class BinaryOperatorAST : public BaseAST {
    const char op;
    std::unique_ptr<BaseAST> left, right;

    public:
        BinaryOperatorAST (char op, std::unique_ptr<BaseAST> left, std::unique_ptr<BaseAST> right) : op(op), left(std::move(left)), right(std::move(right)) {}        
};

// class for when a function is called
class TaskCallAST : public BaseAST {
    const std::string callsTo;
    std::vector<std::unique_ptr<BaseAST>> args;

    public:
        TaskCallAST (const std::string callsTo&, std::vector<std::unique_ptr<BaseAST>> args) : callsTo(callsTo), args(std::move(args)) {}
};

// class for the function header (definition)
class TaskHeaderAST : public BaseAST {
    const std::string name;
    std::vector<std::unique_ptr<BaseAST>> args;

    public:
        TaskHeaderAST (const std::string name&, std::vector<std::unique_ptr<BaseAST>> args) : name(name), args(std::move(args)) {}
};

// class for the function definition
class TaskAST : public BaseAST {
    std::unique_ptr<TaskHeaderAST> header;
    std::unique_ptr<BaseAST> body;

    public:
        TaskAST (std::unique_ptr<TaskHeaderAST> header, std::unique_ptr<BaseAST> body) : header(std::move(header)), body(std::move(body)) {}
};