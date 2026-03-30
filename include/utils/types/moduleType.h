#pragma once
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class ModuleType : public Type {
    Module *_mod;

public:
    explicit ModuleType(Module *m, llvm::SMLoc s, llvm::SMLoc e) : _mod(m), Type(Type::ModulePtr, s, e) {}

    CLASSOF(ModulePtr)

    Module *
    GetMod() const {
        return _mod;
    }

    std::string
    ToString() override {
        return "<module " + _mod->Name + '>';
    }
};

}