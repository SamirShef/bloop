#pragma once
#include <utils/types/type.h>

namespace bloop {

class SizeType : public Type {
    bool _isUnsigned;

public:
    explicit SizeType(bool iu, llvm::SMLoc s, llvm::SMLoc e) : _isUnsigned(iu), Type(Type::Size, s, e) {}

    bool
    IsUnsigned() const {
        return _isUnsigned;
    }

    std::string
    ToString() override {
        return (_isUnsigned ? "u" : "i") + std::string("size");
    }
};

}