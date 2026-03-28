#pragma once
#include <utils/symbols/function.h>
#include <utils/symbols/access.h>
#include <string>
#include <vector>

namespace bloop {

struct Method {
    Function        Func;
    bool            IsStatic;
    AccessModifier  Access;
    uint            RefsCount = 0;

    explicit Method(Function f, bool is, AccessModifier a) : Func(f), IsStatic(is), Access(a) {}
};

struct Trait {
    std::string         Name;
    std::vector<Method> Methods;
    AccessModifier      Access;
    uint                RefsCount = 0;

    explicit Trait(std::string n, std::vector<Method> &m, AccessModifier a) : Name(n), Methods(m), Access(a) {}
};

}