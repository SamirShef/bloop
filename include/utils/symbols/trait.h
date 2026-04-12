#pragma once
#include <utils/symbols/function.h>
#include <utils/symbols/access.h>
#include <vector>

namespace bloop {

struct Method {
    Function        Func;
    bool            IsStatic;
    AccessModifier  Access;
    uint            RefsCount = 0;

    explicit Method(Function f, bool is, AccessModifier a) : Func(f), IsStatic(is), Access(a) {}

    bool
    operator==(const Method &rhs) const {
        return Func == rhs.Func && IsStatic == rhs.IsStatic && Access == rhs.Access;
    }
};

struct Trait {
    NameObj             Name;
    std::vector<Method> Methods;
    AccessModifier      Access;
    uint                RefsCount = 0;

    explicit Trait(NameObj n, std::vector<Method> &m, AccessModifier a) : Name(n), Methods(m), Access(a) {}

    bool
    operator==(const Trait &rhs) const {
        return Name.Name == rhs.Name.Name && Methods == rhs.Methods && Access == rhs.Access;
    }
};

}