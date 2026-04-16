#pragma once
#include <ast/expr.h>

namespace bloop {

class RefExpr : public Expr {
    Expr *_base;

public:
    explicit RefExpr(Expr *b) : _base(b), Expr(NkRefExpr, b->GetStartLoc(), b->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkRefExpr;
    }

    Expr *
    GetBase() const {
        return _base;
    }
};

}