#pragma once
#include <utils/name.h>
#include <utils/symbols/access.h>
#include <utils/value.h>

namespace bloop {

struct Variable {
    NameObj         Name;
    class Type     *Type;
    bool            IsConst;
    AccessModifier  Access;
    uint            RefsCount = 0;
    Value           Val;

    explicit Variable(NameObj n, class Type *t, bool ic, AccessModifier a, Value v) : Name(n), Type(t), IsConst(ic), Access(a), Val(v) {}
};

}