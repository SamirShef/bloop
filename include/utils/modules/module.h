#pragma once
#include <utils/symbols/trait.h>
#include <utils/symbols/function.h>
#include <utils/symbols/variable.h>
#include <utils/symbols/struct.h>
#include <string>
#include <unordered_map>

namespace bloop {

#define HASH(t, n) std::unordered_map<std::string, t> n;

struct Module {
    std::string         Name;
    AccessModifier      Access;
    HASH(Variable,      Vars);
    HASH(FuncOverload,  FuncOverloads);
    HASH(Struct,        Structs);
    HASH(Trait,         Traits);
    HASH(Module *,      Submods);
    HASH(Module *,      Imports);

    explicit Module(std::string n, AccessModifier a) : Name(n), Access(a) {}
};

#undef HASH

}