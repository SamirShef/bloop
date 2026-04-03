#pragma once
#include <utils/name.h>
#include <ast/expr.h>

namespace bloop {

class FieldExpr : public Expr {
    Expr *_base;
    NameObj _name;

public:
    explicit FieldExpr(Expr *b, NameObj n) : _base(b), _name(n), Expr(NkFieldExpr, b->GetStartLoc(), n.End) {}

    Expr *
    GetBase() const {
        return _base;
    }

    NameObj
    GetName() const {
        return _name;
    }
};

}