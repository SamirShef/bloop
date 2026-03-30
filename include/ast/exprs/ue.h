#pragma once
#include <lexer/token.h>
#include <ast/expr.h>

namespace bloop {

class UnaryExpr : public Expr {
    const Token _op;
    Expr *_rhs;

public:
    explicit UnaryExpr(const Token o, Expr *r, llvm::SMLoc s, llvm::SMLoc e) : _op(o), _rhs(r), Expr(NkUnaryExpr, s, e) {}

    const Token
    GetOp() const {
        return _op;
    }

    Expr *
    GetRHS() const {
        return _rhs;
    }
};

}