#pragma once
#include <utils/types/type.h>

namespace bloop {

class FloatingType : public Type {
public:
    enum FloatingKind : bool {
        Float,
        Double
    };

private:
    FloatingKind _kind;

public:
    explicit FloatingType(FloatingKind k, llvm::SMLoc s, llvm::SMLoc e) : _kind(k), Type(Type::Floating, s, e) {}

    bool
    IsFloat() const {
        return _kind == Float;
    }

    bool
    IsDouble() const {
        return _kind == Double;
    }

    std::string
    ToString() override {
        return _kind == Float ? "f32" : "f64";
    }
};

}