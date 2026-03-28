#pragma once
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class TraitType : public Type {
    Module *_baseMod;

public:
    explicit TraitType(Module *b) : _baseMod(b), Type(Type::TraitPtr) {}

    Module *
    GetBaseMod() {
        return _baseMod;
    }
};

}