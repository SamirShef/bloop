#pragma once
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class StructType : public Type {
    Module *_baseMod;

public:
    explicit StructType(Module *m) : _baseMod(m), Type(Type::StructPtr) {}

    Module *
    GetBaseMod() {
        return _baseMod;
    }
};

}