#pragma once
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <string>

namespace bloop {

struct Variable {
    std::string Name;
    class Type *Type;
    AccessModifier Access;

    explicit Variable(std::string n, class Type *t, AccessModifier a) : Name(n), Type(t), Access(a) {}
};

}