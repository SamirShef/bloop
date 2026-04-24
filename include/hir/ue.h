#pragma once
#include <lexer/token.h>
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

enum HIRUnaryKind : uint8_t {
    HIRUkNot,
    HIRUkMinus
};
    
class HIRUnaryExpr : public HIRNode {
    HIRNode *_rhs;
    Type *_type;
    HIRUnaryKind _op;

public:
    explicit HIRUnaryExpr(HIRNode *r, Type *t, HIRUnaryKind o) : _rhs(r), _type(t), _op(o), HIRNode(HIRNkUnaryExpr) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkUnaryExpr;
    }

    HIRNode *
    GetRHS() const {
        return _rhs;
    }

    Type *
    GetType() const {
        return _type;
    }

    HIRUnaryKind
    GetOp() const {
        return _op;
    }
};

}