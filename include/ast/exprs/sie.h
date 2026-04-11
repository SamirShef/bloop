#pragma once
#include <utils/name.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class StructInstanceExpr : public Expr {
    NameObj _name;
    std::vector<std::pair<NameObj, Expr *>> _fields;

public:
    explicit StructInstanceExpr(NameObj n, std::vector<std::pair<NameObj, Expr *>> &f, llvm::SMLoc e)
        : _name(n), _fields(f), Expr(NkStructInstanceExpr, n.Start, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkStructInstanceExpr;
    }

    NameObj
    GetName() const {
        return _name;
    }

    std::vector<std::pair<NameObj, Expr *>> &
    GetFields() {
        return _fields;
    }
};

}