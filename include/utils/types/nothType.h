#pragma once
#include <utils/types/type.h>

namespace bloop {

class NothType : public Type {
public:
    explicit NothType(llvm::SMLoc s, llvm::SMLoc e) : Type(Type::Noth, s ,e) {}

    std::string
    ToString() override {
        return "noth";
    }
};

}