#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRFieldExpr : public HIRNode {
    HIRNode *_base;
    Type *_baseType;
    int _index;

public:
    HIRFieldExpr(HIRNode *b, Type *bt, int i) : _base(b), _baseType(bt), _index(i), HIRNode(HIRNkFieldExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkFieldExpr;
    }
    
    HIRNode *
    GetBase() const {
        return _base;
    }

    Type *
    GetBaseType() const {
        return _baseType;
    }

    int
    GetIndex() const {
        return _index;
    }
};

}