#pragma once
#include <utils/symbols/trait.h>
#include <utils/symbols/access.h>
#include <utils/symbols/variable.h>
#include <string>
#include <vector>

namespace bloop {

struct Field {
    Variable        Var;
    bool            IsStatic;
    AccessModifier  Access;
    uint            RefsCount = 0;

    explicit Field(Variable v, bool is, AccessModifier a) : Var(v), IsStatic(is), Access(a) {}
};

struct Struct {
    std::string         Name;
    std::vector<Field>  Fields;
    std::vector<Method> Methods {};
    AccessModifier      Access;
    uint                RefsCount = 0;

    explicit Struct(std::string n, std::vector<Field> &f, AccessModifier a) : Name(n), Fields(f), Access(a) {}
};

}