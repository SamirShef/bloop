#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRDeref : public HIRNode {
    HIRNode *_base;
    Type *_baseType;

public:
    explicit HIRDeref(HIRNode *b, Type *bt) : _base(b), _baseType(bt), HIRNode(HIRNkDeref) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkDeref;
    }

    HIRNode *
    GetBase() const {
        return _base;
    }

    Type *
    GetBaseType() const {
        return _baseType;
    }
};

}