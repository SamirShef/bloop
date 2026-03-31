#pragma once
#include <utils/symbols/storageKind.h>
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
    StorageKind     Storage;
    int             Index = -1;

    explicit Variable(NameObj n, class Type *t, bool ic, AccessModifier a, Value v, StorageKind s = Static, int i = -1)
        : Name(n), Type(t), IsConst(ic), Access(a), Val(v), Storage(s), Index(i) {}
};

}