#pragma once
#include <ast/stmt.h>
#include <ast/exprs/mce.h>

namespace bloop {

class MethodCallStmt : public Stmt {
    MethodCallExpr *_mce;

public:
    explicit MethodCallStmt(MethodCallExpr *m) : _mce(m), Stmt(Priv, NkMethodCallStmt, m->GetStartLoc(), m->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkMethodCallStmt;
    }

    MethodCallExpr *
    GetMCE() const {
        return _mce;
    }
};

}