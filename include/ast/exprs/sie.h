#pragma once
#include <utils/name.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class StructInstanceExpr : public Expr {
    std::vector<NameObj> _path;
    std::vector<std::pair<NameObj, Expr *>> _fields;

public:
    explicit StructInstanceExpr(std::vector<NameObj> &n, std::vector<std::pair<NameObj, Expr *>> &f, llvm::SMLoc e)
        : _path(n), _fields(f), Expr(NkStructInstanceExpr, n[0].Start, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkStructInstanceExpr;
    }

    std::vector<NameObj> &
    GetPath() {
        return _path;
    }

    std::vector<std::pair<NameObj, Expr *>> &
    GetFields() {
        return _fields;
    }
};

}