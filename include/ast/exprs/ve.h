#pragma once
#include <utils/name.h>
#include <ast/expr.h>

namespace bloop {

class VarExpr : public Expr {
    NameObj _name;

public:
    explicit VarExpr(NameObj n) : _name(n), Expr(NkVarExpr, n.Start, n.End) {}

    void
    Delete() override {}

    NameObj
    GetName() const {
        return _name;
    }
};

}