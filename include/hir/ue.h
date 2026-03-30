#pragma once
#include <lexer/token.h>
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRUnaryExpr : public HIRNode {
    HIRNode *_rhs;
    TokenKind _op;

public:
    explicit HIRUnaryExpr(HIRNode *r, TokenKind o) : _rhs(r), _op(o), HIRNode(HIRNkUnaryExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkUnaryExpr;
    }

    HIRNode *
    GetRHS() const {
        return _rhs;
    }

    TokenKind
    GetOp() const {
        return _op;
    }
};

}