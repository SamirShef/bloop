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
    Module             *Parent;
    AccessModifier      Access;
    HASH(Variable,      Vars);
    HASH(FuncOverload,  FuncOverloads);
    HASH(Struct,        Structs);
    HASH(Trait,         Traits);
    HASH(Module *,      Submods);
    HASH(Module *,      Imports);

    explicit Module(std::string n, AccessModifier a, Module *p = nullptr) : Name(n), Access(a), Parent(p) {}

    bool
    operator==(const Module &rhs) const {
        bool equals = Name == rhs.Name && Access == rhs.Access && Vars == rhs.Vars && FuncOverloads == rhs.FuncOverloads && Structs == rhs.Structs
                   && Traits == rhs.Traits && Submods == rhs.Submods && Imports == rhs.Imports;
        if (Parent && rhs.Parent) {
            return *Parent == *rhs.Parent && equals;
        }
        else if (!Parent && !rhs.Parent) {
            return equals;
        }
        return false;
    }
    
    std::string
    ToString() const {
        if (!Parent) {
            return Name;
        }
        return Parent->ToString() + "." + Name;
    }
};

#undef HASH

}