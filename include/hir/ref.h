#pragma once
#include <hir/node.h>

namespace bloop {

class HIRRef : public HIRNode {
    HIRNode *_base;

public:
    explicit HIRRef(HIRNode *b) : _base(b), HIRNode(HIRNkRef) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkRef;
    }

    HIRNode *
    GetBase() const {
        return _base;
    }
};

}