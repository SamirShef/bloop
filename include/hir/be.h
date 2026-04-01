#pragma once
#include <lexer/token.h>
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

enum HIRBinaryKind : uint8_t {
    HIRBkAdd,
    HIRBkSub,
    HIRBkMul,
    HIRBkDiv,
    HIRBkRem,
    HIRBkEq,
    HIRBkNEq,
    HIRBkGt,
    HIRBkGtEq,
    HIRBkLt,
    HIRBkLtEq,
    HIRBkAnd,
    HIRBkOr
};
    
class HIRBinaryExpr : public HIRNode {
    Type *_commonType;
    HIRNode *_lhs;
    HIRNode *_rhs;
    HIRBinaryKind _op;

public:
    explicit HIRBinaryExpr(Type *ct, HIRNode *l, HIRNode *r, HIRBinaryKind o) : _commonType(ct), _lhs(l), _rhs(r), _op(o), HIRNode(HIRNkBinaryExpr) {}

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

    HIRBinaryKind
    GetOp() const {
        return _op;
    }
};

}