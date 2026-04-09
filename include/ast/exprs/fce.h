#pragma once
#include <utils/name.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class FuncCallExpr : public Expr {
    NameObj _name;
    std::vector<Expr *> _args;

public:
    explicit FuncCallExpr(NameObj n, std::vector<Expr *> &a, llvm::SMLoc e) : _name(n), _args(a), Expr(NkFuncCallExpr, n.Start, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkFuncCallExpr;
    }

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