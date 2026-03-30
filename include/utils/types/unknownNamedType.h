#pragma once
#include <utils/name.h>
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class UnknownNamedType : public Type {
    NameObj _name;

public:
    explicit UnknownNamedType(NameObj n, llvm::SMLoc s, llvm::SMLoc e) : _name(n), Type(Type::Unknown, s, e) {}

    CLASSOF(Unknown)

    NameObj
    GetName() const {
        return _name;
    }
    
    std::string
    ToString() override {
        return "<unknown>";
    }
};

}