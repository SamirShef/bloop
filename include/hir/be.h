#pragma once
#include <lexer/token.h>
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRBinaryExpr : public HIRNode {
    Type *_commonType;
    HIRNode *_lhs;
    HIRNode *_rhs;
    TokenKind _op;

public:
    explicit HIRBinaryExpr(Type *ct, HIRNode *l, HIRNode *r, TokenKind o) : _commonType(ct), _lhs(l), _rhs(r), _op(o), HIRNode(HIRNkBinaryExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkBinaryExpr;
    }

    Type *
    GetCommonType() const {
        return _commonType;
    }

    HIRNode *
    GetLHS() const {
        return _lhs;
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