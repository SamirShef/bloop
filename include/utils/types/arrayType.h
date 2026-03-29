#pragma once
#include <utils/types/type.h>
#include <ast/expr.h>

namespace bloop {

class ArrayType : public Type {
    Type *_base;
    Expr *_size; // requires inference if nullptr

public:
    explicit ArrayType(Type *t, Expr *si, llvm::SMLoc s, llvm::SMLoc e) : _base(t), _size(si), Type(Type::Array, s, e) {}

    Type *
    GetBaseType() const {
        return _base;
    }

    Expr *GetSize() const {
        return _size;
    }

    std::string
    ToString() override {
        return '[' + _base->ToString() + ']';
    }
};

}