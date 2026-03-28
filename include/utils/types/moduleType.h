#pragma once
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class ModuleType : public Type {
    Module *_mod;

public:
    explicit ModuleType(Module *m) : _mod(m), Type(Type::ModulePtr) {}

    Module *
    GetMod() {
        return _mod;
    }
};

}