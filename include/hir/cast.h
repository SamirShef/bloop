#pragma once
#include <hir/node.h>

namespace bloop {

enum CastKind {
    IntToFloat, // sitofp
    FloatToInt, // fptosi
    SignExtend, // sext
    ZeroExtend, // zext
    Truncate,   // trunc
    Bitcast,    // bitcast
};

class HIRCastNode : public HIRNode {
    CastKind _kind;
    HIRNode *_expr;

public:
    explicit HIRCastNode(CastKind k, HIRNode *e) : _kind(k), _expr(e), HIRNode(HIRNkCast) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkCast;
    }

    CastKind
    GetCastKind() const {
        return _kind;
    }

    HIRNode *
    GetExpr() const {
        return _expr;
    }
};

}