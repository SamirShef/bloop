#pragma once
#include <utils/types/type.h>

namespace bloop {

class StringType : public Type {
public:
    explicit StringType(llvm::SMLoc s, llvm::SMLoc e) : Type(Type::String, s, e) {}

    std::string
    ToString() override {
        return "string";
    }
};

}