#pragma once
#include <sstream>
#include <utils/types/type.h>
#include <vector>

namespace bloop {

class TupleType : public Type {
    std::vector<Type *> _types;

public:
    explicit TupleType(std::vector<Type *> &t, llvm::SMLoc s, llvm::SMLoc e) : _types(t), Type(Type::Tuple, s, e) {}

    CLASSOF(Tuple)

    std::vector<Type *> &
    GetTypes() {
        return _types;
    }
    
    int
    GetTypesCount() const {
        return _types.size();
    }

    std::string
    ToString() override {
        std::stringstream ss;
        ss << '(';
        for (auto &t : _types) {
            ss << t->ToString();
        }
        ss << ')';
        return ss.str();
    }
};

}