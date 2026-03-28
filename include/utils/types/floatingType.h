#pragma once
#include <utils/types/type.h>

namespace bloop {

class FloatingType : public Type {
    enum FloatingKind {
        Float,
        Double
    } _kind;

public:
    explicit FloatingType(FloatingKind k) : _kind(k), Type(Type::Floating) {}

    bool
    IsFloat() const {
        return _kind == Float;
    }

    bool
    IsDouble() const {
        return _kind == Double;
    }
};

}