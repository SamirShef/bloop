#pragma once
#include <utils/symbols/access.h>
#include <utils/symbols/variable.h>
#include <string>
#include <vector>

namespace bloop {

struct Field {
    Variable Var;
    AccessModifier Access;
};

struct Struct {
    std::string Name;
    std::vector<Field> Fields;
    AccessModifier Access;

    explicit Struct(std::string n, std::vector<Field> &f, AccessModifier a) : Name(n), Fields(f), Access(a) {}
};

}