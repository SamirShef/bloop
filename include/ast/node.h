#pragma once
#include <cstdint>
#include <llvm/Support/SMLoc.h>

namespace bloop {

enum NodeKind : uint8_t {
    NkStartStmts,
    NkVarDeclStmt,
    NkVarAsgnStmt,
    NkDerefAsgnStmt,
    NkFieldAsgnStmt,
    NkFuncDeclStmt,
    NkFuncCallStmt,
    NkUsingStmt,
    NkRetStmt,
    NkIfElseStmt,
    NkForLoopStmt,
    NkBreakStmt,
    NkContinueStmt,
    NkMethodCallStmt,
    NkStructDeclStmt,
    NkImplStmt,
    NkDelStmt,
    NkEndStmts,

    NkStartExprs,
    NkLitExpr,
    NkVarExpr,
    NkBinaryExpr,
    NkUnaryExpr,
    NkFieldExpr,
    NkFuncCallExpr,
    NkMethodCallExpr,
    NkStructInstanceExpr,
    NkTypeExpr,
    NkNilExpr,
    NkRefExpr,
    NkDerefExpr,
    NkNewExpr,
    NkEndExprs
};

class Node {
    NodeKind _kind;
    llvm::SMLoc _start;
    llvm::SMLoc _end;

public:
    explicit Node(NodeKind k, llvm::SMLoc s, llvm::SMLoc e) : _kind(k), _start(s), _end(e) {}
    
    NodeKind
    GetKind() const {
        return _kind;
    }
    
    llvm::SMLoc &
    GetStartLoc() {
        return _start;
    }

    llvm::SMLoc &
    GetEndLoc() {
        return _end;
    }
};

}