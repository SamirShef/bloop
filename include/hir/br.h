#pragma once
#include <hir/node.h>

namespace bloop {

class HIRBasicBlock;

class HIRBranch : public HIRNode {
    HIRNode *_cond;
    HIRBasicBlock *_then;
    HIRBasicBlock *_else;

public:
    explicit HIRBranch(HIRBasicBlock *t) : _cond(nullptr), _then(t), _else(nullptr), HIRNode(HIRNkBranch) {}
    explicit HIRBranch(HIRNode *c, HIRBasicBlock *t, HIRBasicBlock *e) : _cond(c), _then(t), _else(e), HIRNode(HIRNkBranch) {}

    HIRNode *
    GetCond() const {
        return _cond;
    }

    HIRBasicBlock *
    GetThen() const {
        return _then;
    }

    HIRBasicBlock *
    GetElse() const {
        return _else;
    }
};

}