#pragma once
#include <ast/expr.h>
#include <ast/stmt.h>
#include <vector>

namespace bloop {

class IfElseStmt : public Stmt {
    Expr *_cond;
    std::vector<Stmt *> _then;
    std::vector<Stmt *> _else;

public:
    explicit IfElseStmt(Expr *c, std::vector<Stmt *> &t, std::vector<Stmt *> &el, llvm::SMLoc s, llvm::SMLoc e)
        : _cond(c), _then(t), _else(el), Stmt(Priv, NkIfElseStmt, s, e) {}
    
    Expr *
    GetCond() const {
        return _cond;
    }

    std::vector<Stmt *> &
    GetThenBranch() {
        return _then;
    }

    std::vector<Stmt *> &
    GetElseBranch() {
        return _else;
    }
};

}