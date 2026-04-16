#pragma once
#include <ast/expr.h>

namespace bloop {

class NewExpr : public Expr {
    Expr *_typeExpr;
    Expr *_expr;

public:
    explicit NewExpr(Expr *te, Expr *e, llvm::SMLoc end) : _typeExpr(te), _expr(e), Expr(NkNewExpr, te->GetStartLoc(), end) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkNewExpr;
    }

    Expr *
    GetTypeExpr() const {
        return _typeExpr;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}