#pragma once
#include <utils/symbols/trait.h>
#include <utils/symbols/access.h>
#include <utils/symbols/variable.h>
#include <vector>

namespace bloop {

struct Field {
    Variable        Var;
    bool            IsStatic;
    AccessModifier  Access;
    uint            RefsCount = 0;

    explicit Field(Variable v, bool is, AccessModifier a) : Var(v), IsStatic(is), Access(a) {}

    bool
    operator==(const Field &rhs) const {
        return Var == rhs.Var && IsStatic == rhs.IsStatic && Access == rhs.Access;
    }
};

struct Struct {
    NameObj                     Name;
    Module                     *Parent;
    std::vector<Field>          Fields;
    std::vector<MethodOverload> Methods {};
    AccessModifier              Access;
    uint                        RefsCount = 0;

    explicit Struct(NameObj n, Module *p, std::vector<Field> &f, AccessModifier a) : Name(n), Parent(p), Fields(f), Access(a) {}

    std::string
    GetMangledName() const;

    bool
    operator==(const Struct &rhs) const {
        return Name.Name == rhs.Name.Name && Fields == rhs.Fields && Methods == rhs.Methods && Access == rhs.Access;
    }
};

}