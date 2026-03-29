#pragma once
#include <utils/name.h>
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class StructType : public Type {
    NameObj _name;
    Module *_baseMod;

public:
    explicit StructType(NameObj n, Module *m, llvm::SMLoc s, llvm::SMLoc e) : _name(n), _baseMod(m), Type(Type::StructPtr, s, e) {}

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
        return "<struct " + _name.Name + '>';
    }
};

}