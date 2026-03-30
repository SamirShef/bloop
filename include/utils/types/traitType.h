#pragma once
#include <utils/name.h>
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class TraitType : public Type {
    NameObj _name;
    Module *_baseMod;

public:
    explicit TraitType(NameObj n, Module *b, llvm::SMLoc s, llvm::SMLoc e) : _name(n), _baseMod(b), Type(Type::TraitPtr, s, e) {}

    CLASSOF(TraitPtr)

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
        return "<trait " + _name.Name + '>';
    }
};

}