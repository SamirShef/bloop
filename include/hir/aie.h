#pragma once
#include <utils/types/type.h>
#include <hir/node.h>
#include <vector>

namespace bloop {

class HIRArrayInstanceExpr : public HIRNode {
    Type *_arrType;
    std::vector<HIRNode *> _exprs;

public:
    explicit HIRArrayInstanceExpr(Type *at, std::vector<HIRNode *> &ex)
        : _arrType(at), _exprs(ex), HIRNode(HIRNkArrayInstanceExpr) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkArrayInstanceExpr;
    }

    Type *
    GetArrType() const {
        return _arrType;
    }
    
    std::vector<HIRNode *> &
    GetExprs() {
        return _exprs;
    }
};

}