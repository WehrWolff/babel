#include "ast.h"

std::unique_ptr<BaseAST> name() {
    auto Result = std::make_unique<BaseAST>(NumVal);
    return std::move(Result);
}