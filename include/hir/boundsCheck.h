#pragma once
#include <utils/types/type.h>
#include <hir/node.h>

namespace bloop {

class HIRBoundsCheck : public HIRNode {
    HIRNode *_len;
    HIRNode *_index;
    std::string _pos;
    
public:
    explicit HIRBoundsCheck(HIRNode *l, HIRNode *i, std::string pos) : _len(l), _index(i), _pos(pos), HIRNode(HIRNkBoundsCheck) {}

    constexpr static bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkBoundsCheck;
    }

    HIRNode *
    GetLength() const {
        return _len;
    }

    HIRNode *
    GetIndex() const {
        return _index;
    }

    std::string
    GetPos() const {
        return _pos;
    }
};

}