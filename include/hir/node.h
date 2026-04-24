#pragma once
#include <cstdint>

namespace bloop {

enum HIRNodeKind : uint8_t {
    HIRNkStartStmts,
    HIRNkVarDeclStmt,
    HIRNkFuncDeclStmt,
    HIRNkUsingStmt,
    HIRNkRetStmt,
    HIRNkVarStore,
    HIRNkDerefStore,
    HIRNkFieldStore,
    HIRNkStructDeclStmt,
    HIRNkDelStmt,
    HIRNkArrayStore,
    HIRNkEndStmts,
    HIRNkCast,
    HIRNkBranch,

    HIRNkStartExprs,
    HIRNkLitExpr,
    HIRNkVarExpr,
    HIRNkBinaryExpr,
    HIRNkUnaryExpr,
    HIRNkFieldExpr,
    HIRNkFuncCallExpr,
    HIRNkMethodCallExpr,
    HIRNkStructInstanceExpr,
    HIRNkArrayInstanceExpr,
    HIRNkDeref,
    HIRNkRef,
    HIRNkNilExpr,
    HIRNkNewExpr,
    HIRNkArrayAccessExpr,
    HIRNkEndExprs,

    HIRNkBasicBlock,
    HIRNkNilCheck,
    HIRNkBoundsCheck,
};

class HIRNode {
    HIRNodeKind _kind;

public:
    explicit HIRNode(HIRNodeKind k) : _kind(k) {}
    
    HIRNodeKind
    GetKind() const {
        return _kind;
    }

    bool
    IsTerminator() const {
        return GetKind() == HIRNkRetStmt || GetKind() == HIRNkBranch;
    }
};

}