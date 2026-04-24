#pragma once
#include <ast/expr.h>

namespace bloop {

class ArrayAccessExpr : public Expr {
    Expr *_baseArr;
    Expr *_index;

public:
    explicit ArrayAccessExpr(Expr *b, Expr *i, llvm::SMLoc e) : _baseArr(b), _index(i), Expr(NkArrayAccessExpr, b->GetStartLoc(), e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkArrayAccessExpr;
    }

    Expr *
    GetBase() const {
        return _baseArr;
    }

    Expr *
    GetIndex() const {
        return _index;
    }
};

}