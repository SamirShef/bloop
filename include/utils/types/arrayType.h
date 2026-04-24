#pragma once
#include <utils/types/type.h>
#include <ast/expr.h>

namespace bloop {

class ArrayType : public Type {
    Type *_base;
    Expr *_sizeExpr;
    int64_t _size = 0;

public:
    explicit ArrayType(Type *t, Expr *si, llvm::SMLoc s, llvm::SMLoc e) : _base(t), _sizeExpr(si), Type(Type::Array, s, e) {}

    CLASSOF(Array)

    Type *
    GetBaseType() const {
        return _base;
    }

    void
    SetBaseType(Type *b) {
        _base = b;
    }

    Expr *
    GetSizeExpr() const {
        return _sizeExpr;
    }

    int64_t
    GetSize() const {
        return _size;
    }

    void
    SetSize(int64_t s) {
        _size = s;
    }

    std::string
    ToString() override {
        return '[' + _base->ToString() + ']';
    }
};

}