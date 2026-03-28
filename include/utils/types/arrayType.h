#pragma once
#include <utils/types/type.h>

namespace bloop {

class ArrayType : public Type {
    Type *_base;
    // TODO: add `Expr *_size` field

public:
    explicit ArrayType(Type *t) : _base(t), Type(Type::Array) {}

    Type *
    GetBaseType() {
        return _base;
    }

    // TODO: add `Expr *GetSize() const;` method
};

}