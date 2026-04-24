#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRArrayAccessExpr : public HIRNode {
    HIRNode *_baseArr;
    Type *_baseType;
    HIRNode *_index;

public:
    explicit HIRArrayAccessExpr(HIRNode *b, Type *bt, HIRNode *i) : _baseArr(b), _baseType(bt), _index(i), HIRNode(HIRNkArrayAccessExpr) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkArrayAccessExpr;
    }

    HIRNode *
    GetBase() const {
        return _baseArr;
    }

    Type *
    GetBaseType() const {
        return _baseType;
    }
    
    HIRNode *
    GetIndex() const {
        return _index;
    }
};

}