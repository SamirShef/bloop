#pragma once
#include <utils/types/type.h>

namespace bloop {

class IntegerType : public Type {
    unsigned _bitWidth;
    bool _isUnsigned;

public:
    explicit IntegerType(unsigned bw, bool iu) : _bitWidth(bw), _isUnsigned(iu), Type(Type::Integer) {}

    unsigned
    GetBitWidth() const {
        return _bitWidth;
    }

    bool
    IsUnsigned() const {
        return _isUnsigned;
    }
};

}