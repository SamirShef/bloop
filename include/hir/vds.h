#pragma once
#include <utils/symbols/storageKind.h>
#include <utils/types/type.h>
#include <hir/node.h>
#include <string>

namespace bloop {

class HIRVarDeclStmt : public HIRNode {
    std::string _name;
    Type *_type;
    HIRNode *_expr;
    bool _isConst;
    StorageKind _storage;

public:
    explicit HIRVarDeclStmt(std::string n, Type *t, HIRNode *e, bool ic, StorageKind k) : _name(n), _type(t), _expr(e), _isConst(ic), _storage(k), HIRNode(HIRNkVarDeclStmt) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkVarDeclStmt;
    }

    std::string
    GetName() const {
        return _name;
    }

    Type *
    GetType() const {
        return _type;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }

    bool
    IsConst() const {
        return _isConst;
    }

    StorageKind
    GetStorageKind() const {
        return _storage;
    }
};

}