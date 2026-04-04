#pragma once
#include <utils/types/type.h>

namespace bloop {

class PointerType : public Type {
    Type *_base;

public:
    explicit PointerType(Type *t, llvm::SMLoc s, llvm::SMLoc e) : _base(t), Type(Type::Pointer, s, e) {}

    CLASSOF(Pointer)

    Type *
    GetBaseType() const {
        return _base;
    }

    void
    SetBaseType(Type *b) {
        _base = b;
    }

    std::string
    ToString() override {
        return "*" + _base->ToString();
    }
};

}