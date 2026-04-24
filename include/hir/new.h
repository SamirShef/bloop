#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRNewExpr : public HIRNode {
    Type *_type;
    HIRNode *_expr;

public:
    explicit HIRNewExpr(Type *t, HIRNode *e) : _type(t), _expr(e), HIRNode(HIRNkNewExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkNewExpr;
    }

    Type *
    GetType() const {
        return _type;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}