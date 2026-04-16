#pragma once
#include <hir/node.h>

namespace bloop {

class HIRDelStmt : public HIRNode {
    HIRNode *_expr;

public:
    HIRDelStmt(HIRNode *e) : _expr(e), HIRNode(HIRNkDelStmt) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkDelStmt;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}