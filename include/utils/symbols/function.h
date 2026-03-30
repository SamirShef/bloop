#pragma once
#include <utils/name.h>
#include <ast/expr.h>
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <vector>

namespace bloop {

struct Argument {
    NameObj Name;
    class Type *Type;
    Expr       *DefaultVal;

    explicit Argument(NameObj n, class Type *t, Expr *dv = nullptr) : Name(n), Type(t), DefaultVal(dv) {}
};

struct Function {
    NameObj                 Name;
    Type                   *RetType;
    std::vector<Argument>   Args;
    AccessModifier          Access;
    uint                    RefsCount = 0;

    explicit Function(NameObj n, Type *rt, std::vector<Argument> &ar, AccessModifier ac) : Name(n), RetType(rt), Args(ar), Access(ac) {}
};

struct FuncOverload {
    std::vector<Function> Candidates;
};

}