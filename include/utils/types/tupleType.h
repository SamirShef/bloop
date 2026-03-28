#pragma once
#include <utils/types/type.h>
#include <vector>

namespace bloop {

class TupleType : public Type {
    std::vector<Type *> _types;

public:
    explicit TupleType(std::vector<Type *> &t) : _types(t), Type(Type::Tuple) {}

    std::vector<Type *> &
    GetTypes() {
        return _types;
    }
    
    int
    GetTypesCount() const {
        return _types.size();
    }
};

}