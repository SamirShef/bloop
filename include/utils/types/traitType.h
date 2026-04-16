#pragma once
#include <utils/name.h>
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class TraitType : public Type {
    NameObj _name;
    Module *_baseMod;

public:
    explicit TraitType(NameObj n, Module *b, llvm::SMLoc s, llvm::SMLoc e) : _name(n), _baseMod(b), Type(Type::Trait, s, e) {}

    CLASSOF(Trait)

    NameObj
    GetName() const {
        return _name;
    }
    
    Module *
    GetBaseMod() const {
        return _baseMod;
    }

    std::string
    ToString() override {
        return _name.Name;
    }
};

}