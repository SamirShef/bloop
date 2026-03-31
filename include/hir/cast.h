#pragma once
#include <utils/types/type.h>
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
    Type *_from;
    Type *_to;

public:
    explicit HIRCastNode(CastKind k, HIRNode *e, Type *f, Type *t) : _kind(k), _expr(e), _from(f), _to(t), HIRNode(HIRNkCast) {}

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

    Type *
    GetFromType() const {
        return _from;
    }

    Type *
    GetToType() const {
        return _to;
    }
};

}