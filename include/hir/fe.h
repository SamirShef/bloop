#pragma once
#include <hir/node.h>

namespace bloop {

class HIRFieldExpr : public HIRNode {
    HIRNode *_base;
    int _index;

public:
    HIRFieldExpr(HIRNode *b, int i) : _base(b), _index(i), HIRNode(HIRNkFieldExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkFieldExpr;
    }
    
    HIRNode *
    GetBase() const {
        return _base;
    }

    int
    GetIndex() const {
        return _index;
    }
};

}