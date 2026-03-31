#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRRetStmt : public HIRNode {
    Type *_type;
    HIRNode *_expr;

public:
    explicit HIRRetStmt(Type *t, HIRNode *e) : _type(t), _expr(e), HIRNode(HIRNkRetStmt) {}

    Type *
    GetType() const {
        return _type;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}