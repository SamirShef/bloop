#pragma once
#include <utils/value.h>
#include <hir/node.h>

namespace bloop {

class HIRLiteralExpr : public HIRNode {
    Value _val;

public:
    explicit HIRLiteralExpr(Value v) : _val(v), HIRNode(HIRNkLitExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkLitExpr;
    }

    Value
    GetVal() const {
        return _val;
    }
};

}