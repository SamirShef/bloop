#pragma once
#include <ast/expr.h>

namespace bloop {

class DerefExpr : public Expr {
    Expr *_base;

public:
    explicit DerefExpr(Expr *b) : _base(b), Expr(NkDerefExpr, b->GetStartLoc(), b->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkDerefExpr;
    }

    Expr *
    GetBase() const {
        return _base;
    }
};

}