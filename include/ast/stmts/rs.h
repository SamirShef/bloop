#pragma once
#include <ast/expr.h>
#include <ast/stmt.h>

namespace bloop {

class RetStmt : public Stmt {
    Expr *_expr; // maybe nullptr

public:
    RetStmt(Expr *ex, llvm::SMLoc s, llvm::SMLoc e) : _expr(ex), Stmt(Priv, NkRetStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkRetStmt;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}