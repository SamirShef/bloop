#pragma once
#include <utils/types/type.h>

namespace bloop {

class StringType : public Type {
public:
    explicit StringType() : Type(Type::String) {}
};

}