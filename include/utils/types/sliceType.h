#pragma once
#include <utils/types/type.h>

namespace bloop {

class SliceType : public Type {
    Type *_base;

public:
    explicit SliceType(Type *t, llvm::SMLoc s, llvm::SMLoc e) : _base(t), Type(Type::Slice, s, e) {}

    CLASSOF(Slice)

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
        return "[" + _base->ToString() + ']';
    }
};

}