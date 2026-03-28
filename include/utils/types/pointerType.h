#pragma once
#include <utils/types/type.h>

namespace bloop {

class PointerType : public Type {
    Type *_base;

public:
    explicit PointerType(Type *t) : _base(t), Type(Type::Pointer) {}

    Type *
    GetBaseType() {
        return _base;
    }
};

}