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

struct MethodOverload {
    std::vector<Method> Candidates;

    bool
    operator==(const MethodOverload &rhs) const {
        return Candidates == rhs.Candidates;
    }
};

struct Trait {
    NameObj                     Name;
    std::vector<MethodOverload> Methods;
    AccessModifier              Access;
    uint                        RefsCount = 0;

    explicit Trait(NameObj n, std::vector<MethodOverload> &m, AccessModifier a) : Name(n), Methods(m), Access(a) {}

    bool
    operator==(const Trait &rhs) const {
        return Name.Name == rhs.Name.Name && Methods == rhs.Methods && Access == rhs.Access;
    }
};

}