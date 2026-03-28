#pragma once
#include <utils/symbols/function.h>
#include <utils/symbols/access.h>
#include <string>
#include <vector>

namespace bloop {

struct Method {
    Function Func;
    AccessModifier Access;
};

struct Trait {
    std::string Name;
    std::vector<Method> Methods;
    AccessModifier Access;

    explicit Trait(std::string n, std::vector<Method> &m, AccessModifier a) : Name(n), Methods(m), Access(a) {}
};

}