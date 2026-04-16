#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRNilCheck : public HIRNode {
    HIRNode *_ptr;
    std::string _pos;
    
public:
    explicit HIRNilCheck(HIRNode *p, std::string pos) : _ptr(p), _pos(pos), HIRNode(HIRNkNilCheck) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkNilCheck;
    }

    HIRNode *
    GetPtr() const {
        return _ptr;
    }

    std::string
    GetPos() const {
        return _pos;
    }
};

}