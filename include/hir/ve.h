#pragma once
#include <utils/symbols/storageKind.h>
#include <utils/modules/module.h>
#include <hir/node.h>

namespace bloop {

class HIRVarExpr : public HIRNode {
    StorageKind _kind;
    int _index;
    Module *_parent;

public:
    HIRVarExpr(StorageKind k, int i, Module *p = nullptr) : _kind(k), _index(i), _parent(p), HIRNode(HIRNkVarExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkVarExpr;
    }

    StorageKind
    GetStorageKind() const {
        return _kind;
    }

    int
    GetIndex() const {
        return _index;
    }

    Module *
    GetParentMod() const {
        return _parent;
    }
};

}