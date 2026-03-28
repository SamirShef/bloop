#pragma once
#include <utils/types/type.h>

namespace bloop {

class CharType : public Type {
public:
    explicit CharType() : Type(Type::Char) {}
};

}