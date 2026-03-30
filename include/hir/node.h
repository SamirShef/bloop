#pragma once
#include <cstdint>

namespace bloop {

enum HIRNodeKind : uint8_t {
    HIRNkStartStmts,
    HIRNkVarDeclStmt,
    HIRNkFuncDeclStmt,
    HIRNkUsingStmt,
    HIRNkRetStmt,
    HIRNkEndStmts,
    HIRNkCast,

    HIRNkStartExprs,
    HIRNkLitExpr,
    HIRNkVarExpr,
    HIRNkBinaryExpr,
    HIRNkUnaryExpr,
    HIRNkEndExprs
};

class HIRNode {
    HIRNodeKind _kind;

public:
    explicit HIRNode(HIRNodeKind k) : _kind(k) {}
    
    HIRNodeKind
    GetKind() const {
        return _kind;
    }
};

}