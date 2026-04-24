#pragma once
#include <ast/expr.h>
#include <ast/stmt.h>

namespace bloop {

class DelStmt : public Stmt {
    Expr *_expr;
    
public:
    DelStmt(Expr *e) : _expr(e), Stmt(Priv, NkDelStmt, e->GetStartLoc(), e->GetEndLoc()) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkDelStmt;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}