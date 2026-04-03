#pragma once
#include <utils/name.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class MethodCallExpr : public Expr {
    Expr *_base;
    NameObj _name;
    std::vector<Expr *> _args;

public:
    explicit MethodCallExpr(Expr *b, NameObj n, std::vector<Expr *> &a, llvm::SMLoc e)
        : _base(b), _name(n), _args(a), Expr(NkMethodCallExpr, b->GetStartLoc(), e) {}

    NameObj
    GetName() const {
        return _name;
    }

    std::vector<Expr *> &
    GetArgs() {
        return _args;
    }
};

}