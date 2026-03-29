#pragma once
#include <lexer/token.h>
#include <ast/expr.h>

namespace bloop {

class BinaryExpr : public Expr {
    Expr *_lhs;
    const Token _op;
    Expr *_rhs;

public:
    explicit BinaryExpr(Expr *l, const Token o, Expr *r, llvm::SMLoc s, llvm::SMLoc e) : _lhs(l), _op(o), _rhs(r), Expr(NkBinaryExpr, s, e) {}

    void
    Delete() override {
        _lhs->~Expr();
        _rhs->~Expr();
    }

    Expr *
    GetLHS() const {
        return _lhs;
    }

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