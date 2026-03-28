#pragma once
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <string>
#include <vector>

namespace bloop {

struct Argument {
    std::string Name;
    class Type *Type;
    // TODO: add value field (if argument was initialized by default value)

    explicit Argument(std::string n, class Type *t) : Name(n), Type(t) {}
};

struct Function {
    std::string Name;
    Type *RetType;
    std::vector<Argument> Args;
    AccessModifier Access;

    explicit Function(std::string n, Type *rt, std::vector<Argument> &ar, AccessModifier ac) : Name(n), RetType(rt), Args(ar), Access(ac) {}
};

struct FuncOverload {
    std::vector<Function> Candidates;
};

}