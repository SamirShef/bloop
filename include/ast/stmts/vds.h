#pragma once
#include <utils/types/type.h>
#include <utils/name.h>
#include <ast/stmt.h>
#include <ast/expr.h>

namespace bloop {

class VarDeclStmt : public Stmt {
    NameObj _name;
    Type *_type; // requires inference if nullptr
    Expr *_expr;
    bool _isConst;

public:
    explicit VarDeclStmt(NameObj n, Type *t, Expr *expr, bool ic, AccessModifier a, llvm::SMLoc s, llvm::SMLoc e)
        : _name(n), _type(t), _expr(expr), _isConst(ic), Stmt(a, NkVarDeclStmt, s, e) {}

    void
    Delete() override {
        delete _type;
        delete _expr;
    }

    NameObj
    GetName() const {
        return _name;
    }

    Type *
    GetType() const {
        return _type;
    }

    Expr *
    GetExpr() const {
        return _expr;
    }

    bool
    IsConst() const {
        return _isConst;
    }
};

}