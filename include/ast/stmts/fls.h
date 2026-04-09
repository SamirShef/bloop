#pragma once
#include <ast/stmt.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class ForLoopStmt : public Stmt {
    Stmt *_indexator;
    Expr *_cond;
    Stmt *_iteration;
    std::vector<Stmt *> _body;

public:
    explicit ForLoopStmt(Stmt *i, Expr *c, Stmt *it, std::vector<Stmt *> &b, AccessModifier a, llvm::SMLoc s, llvm::SMLoc e)
        : _indexator(i), _cond(c), _iteration(it), _body(b), Stmt(a, NkForLoopStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkForLoopStmt;
    }
    
    Stmt *
    GetIndexator() const {
        return _indexator;
    }
    
    Expr *
    GetCondition() const {
        return _cond;
    }

    Stmt *
    GetIteration() const {
        return _iteration;
    }

    std::vector<Stmt *> &
    GetBody() {
        return _body;
    }
};

}