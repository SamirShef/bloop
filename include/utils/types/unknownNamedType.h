#pragma once
#include <utils/name.h>
#include <utils/modules/module.h>
#include <utils/types/type.h>

namespace bloop {

class UnknownNamedType : public Type {
    std::vector<NameObj> _path;

public:
    explicit UnknownNamedType(std::vector<NameObj> &p, llvm::SMLoc s, llvm::SMLoc e) : _path(p), Type(Type::Unknown, s, e) {}

    CLASSOF(Unknown)

    std::vector<NameObj> &
    GetPath() {
        return _path;
    }
    
    std::string
    ToString() override {
        std::string res = _path[0].Name;
        for (int i = 1; i < _path.size(); ++i) {
            res += '.' + _path[i].Name;
        }
        return res;
    }
};

}