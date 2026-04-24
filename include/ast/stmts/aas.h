#pragma once
#include <ast/stmt.h>
#include <ast/expr.h>

namespace bloop {

class ArrayAsgnStmt : public Stmt {
    Expr *_baseArr;
    Expr *_index;
    Expr *_expr;

public:
    explicit ArrayAsgnStmt(Expr *b, Expr *i, Expr *e, llvm::SMLoc end) : _baseArr(b), _index(i), _expr(e), Stmt(Priv, NkArrayAsgnStmt, b->GetStartLoc(), end) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkArrayAsgnStmt;
    }

    Expr *
    GetBase() const {
        return _baseArr;
    }

    Expr *
    GetIndex() const {
        return _index;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }
};

}