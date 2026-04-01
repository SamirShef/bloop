#pragma once
#include <sys/types.h>
#include <utils/symbols/storageKind.h>
#include <hir/node.h>

namespace bloop {

class HIRVarExpr : public HIRNode {
    StorageKind _kind;
    uint _index;

public:
    HIRVarExpr(StorageKind k, uint i) : _kind(k), _index(i), HIRNode(HIRNkVarExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkVarExpr;
    }

    StorageKind
    GetStorageKind() const {
        return _kind;
    }

    uint
    GetIndex() const {
        return _index;
    }
};

}