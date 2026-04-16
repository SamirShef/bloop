#pragma once
#include <hir/node.h>

namespace bloop {

class HIRDerefStore : public HIRNode {
    HIRNode *_ptr;
    HIRNode *_expr;

public:
    HIRDerefStore(HIRNode *p, HIRNode *e) : _ptr(p), _expr(e), HIRNode(HIRNkDerefStore) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkDerefStore;
    }

    HIRNode *
    GetPtr() const {
        return _ptr;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}