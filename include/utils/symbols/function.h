#pragma once
#include <utils/symbols/storageKind.h>
#include <utils/name.h>
#include <ast/expr.h>
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <vector>

namespace bloop {

class Module;

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
    StorageKind             Storage;
    Module *                Parent = nullptr;
    int                     Index = -1;

    explicit Function(NameObj n, Type *rt, std::vector<Argument> &ar, AccessModifier ac, Module *m = nullptr, StorageKind s = Static, int i = -1)
        : Name(n), RetType(rt), Args(ar), Access(ac), Parent(m), Storage(s), Index(i) {}
    
    std::string
    GetMangledName() const;
};

struct FuncOverload {
    std::vector<Function> Candidates;
};

}