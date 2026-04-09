#pragma once
#include <utils/name.h>
#include <ast/stmt.h>
#include <ast/expr.h>

namespace bloop {

class VarAsgnStmt : public Stmt {
    NameObj _name;
    Expr *_expr;

public:
    explicit VarAsgnStmt(NameObj n, Expr *expr, llvm::SMLoc s, llvm::SMLoc e)
        : _name(n), _expr(expr), Stmt(Priv, NkVarAsgnStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkVarAsgnStmt;
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