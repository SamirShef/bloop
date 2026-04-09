#pragma once
#include <utils/name.h>
#include <ast/stmt.h>
#include <ast/expr.h>

namespace bloop {

class FieldAsgnStmt : public Stmt {
    Expr *_fe;
    NameObj _name;
    Expr *_expr;

public:
    explicit FieldAsgnStmt(Expr *b, NameObj n, Expr *expr, llvm::SMLoc e)
        : _fe(b), _name(n), _expr(expr), Stmt(Priv, NkFieldAsgnStmt, b->GetStartLoc(), e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkFieldAsgnStmt;
    }

    Expr *
    GetBase() const {
        return _fe;
    }

    NameObj
    GetName() const {
        return _name;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}