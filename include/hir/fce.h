#pragma once
#include <hir/node.h>
#include <sys/types.h>
#include <vector>

namespace bloop {

class HIRFuncCallExpr : public HIRNode {
    uint _index;
    std::vector<HIRNode *> _args;

public:
    explicit HIRFuncCallExpr(uint i, std::vector<HIRNode *> &a) : _index(i), _args(a), HIRNode(HIRNkFuncCallExpr) {}

    uint
    GetIndex() const {
        return _index;
    }

    std::vector<HIRNode *> &
    GetArgs() {
        return _args;
    }
};

}