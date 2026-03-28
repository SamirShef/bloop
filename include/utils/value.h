#pragma once
#include <utils/types/type.h>

namespace bloop {

struct Value {
    enum Kind {
        Unknown,
        Const,
        Nil
    } Kind;
    class Type *Type;
};

}