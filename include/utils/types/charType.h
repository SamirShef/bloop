#pragma once
#include <utils/types/type.h>

namespace bloop {

class CharType : public Type {
public:
    explicit CharType(llvm::SMLoc s, llvm::SMLoc e) : Type(Type::Char, s ,e) {}

    std::string
    ToString() override {
        return "char";
    }
};

}