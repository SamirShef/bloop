#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRNilExpr : public HIRNode {
public:
    explicit HIRNilExpr() : HIRNode(HIRNkNilExpr) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkNilExpr;
    }
};

}