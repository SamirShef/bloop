#pragma once
#include <ast/stmt.h>
#include <ast/exprs/fce.h>

namespace bloop {

class FuncCallStmt : public Stmt {
    FuncCallExpr *_fce;

public:
    explicit FuncCallStmt(FuncCallExpr *f) : _fce(f), Stmt(Priv, NkFuncCallStmt, f->GetStartLoc(), f->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkFuncCallStmt;
    }

    FuncCallExpr *
    GetFCE() const {
        return _fce;
    }
};

}