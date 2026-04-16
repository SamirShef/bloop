#pragma once
#include <ast/stmt.h>
#include <ast/expr.h>

namespace bloop {
class DerefAsgnStmt : public Stmt {
    Expr *_base;
    Expr *_expr;

public:
    explicit DerefAsgnStmt(Expr *b, Expr *expr, llvm::SMLoc s, llvm::SMLoc e) : _base(b), _expr(expr), Stmt(Priv, NkDerefAsgnStmt, s, e) {}

    constexpr static bool classof(const Node *node) {
        return node->GetKind() == NkDerefAsgnStmt;
    }

    Expr *
    GetBase() const {
        return _base;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};
}