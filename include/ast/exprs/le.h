#pragma once
#include <utils/value.h>
#include <ast/expr.h>

namespace bloop {

class LiteralExpr : public Expr {
    Value _val;

public:
    explicit LiteralExpr(Value v) : _val(v), Expr(NkLitExpr, v.Start, v.End) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkLitExpr;
    }

    Value
    GetVal() const {
        return _val;
    }
};

}