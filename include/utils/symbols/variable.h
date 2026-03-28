#pragma once
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <string>

namespace bloop {

struct Variable {
    std::string     Name;
    class Type     *Type;
    bool            IsConst;
    AccessModifier  Access;
    uint            RefsCount = 0;

    explicit Variable(std::string n, class Type *t, bool ic, AccessModifier a) : Name(n), Type(t), IsConst(ic), Access(a) {}
};

}