#pragma once
#include <utils/types/type.h>
#include <ast/expr.h>

namespace bloop {

class NewExpr : public Expr {
    Type *_type;
    Expr *_expr;

public:
    explicit NewExpr(Type *t, Expr *e, llvm::SMLoc s, llvm::SMLoc end) : _type(t), _expr(e), Expr(NkNewExpr, s, end) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkNewExpr;
    }

    Type *&
    GetType() {
        return _type;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}