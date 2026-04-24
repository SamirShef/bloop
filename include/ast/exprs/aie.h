#pragma once
#include <ast/expr.h>
#include <vector>

namespace bloop {

class ArrayInstanceExpr : public Expr {
    std::vector<Expr *> _exprs;

public:
    explicit ArrayInstanceExpr(std::vector<Expr *> &ex, llvm::SMLoc s, llvm::SMLoc e)
        : _exprs(ex), Expr(NkArrayInstanceExpr, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkArrayInstanceExpr;
    }

    std::vector<Expr *> &
    GetExprs() {
        return _exprs;
    }
};

}