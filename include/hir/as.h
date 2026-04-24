#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRArrayStore : public HIRNode {
    HIRNode *_baseArr;
    Type *_baseType;
    HIRNode *_index;
    HIRNode *_expr;

public:
    explicit HIRArrayStore(HIRNode *b, Type *bt, HIRNode *i, HIRNode *e) : _baseArr(b), _baseType(bt), _index(i), _expr(e), HIRNode(HIRNkArrayStore) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkArrayStore;
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

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}