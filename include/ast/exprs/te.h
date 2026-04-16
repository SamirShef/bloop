#pragma once
#include <utils/value.h>
#include <ast/expr.h>

namespace bloop {

class TypeExpr : public Expr {
    Type *_type;

public:
    explicit TypeExpr(Type *t) : _type(t), Expr(NkTypeExpr, t->GetStartLoc(), t->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkTypeExpr;
    }

    Type *&
    GetType() {
        return _type;
    }
};

}