#pragma once
#include <utils/types/type.h>

namespace bloop {

class IntegerType : public Type {
    unsigned _bitWidth;
    bool _isUnsigned;

public:
    explicit IntegerType(unsigned bw, bool iu, llvm::SMLoc s, llvm::SMLoc e) : _bitWidth(bw), _isUnsigned(iu), Type(Type::Integer, s, e) {}

    unsigned
    GetBitWidth() const {
        return _bitWidth;
    }

    bool
    IsUnsigned() const {
        return _isUnsigned;
    }

    std::string
    ToString() override {
        if (_bitWidth == 1) {
            return "bool";
        }
        return (_isUnsigned ? "u" : "i") + std::to_string(_bitWidth);
    }
};

}