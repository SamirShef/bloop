#pragma once
#include "utils/symbols/storageKind.h"
#include <hir/node.h>

namespace bloop {

class HIRVarStore : public HIRNode {
    StorageKind _kind;
    int _index;
    HIRNode *_expr;

public:
    HIRVarStore(StorageKind k, int i, HIRNode *e) : _kind(k), _index(i), _expr(e), HIRNode(HIRNkVarStore) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkVarStore;
    }

    StorageKind
    GetStorageKind() const {
        return _kind;
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