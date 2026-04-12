#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRFieldStore : public HIRNode {
    HIRNode *_base;
    Type *_baseType;
    int _index;
    HIRNode *_expr;

public:
    HIRFieldStore(HIRNode *b, Type *bt, int i, HIRNode *e) : _base(b), _baseType(bt), _index(i), _expr(e), HIRNode(HIRNkFieldStore) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkFieldStore;
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

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}