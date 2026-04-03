#pragma once
#include <utils/modules/module.h>
#include <hir/node.h>
#include <sys/types.h>
#include <string>
#include <vector>

namespace bloop {

class HIRFuncCallExpr : public HIRNode {
    std::string _name;
    std::vector<HIRNode *> _args;
    Module *_parent;

public:
    explicit HIRFuncCallExpr(std::string n, std::vector<HIRNode *> &a, Module *p = nullptr) : _name(n), _args(a), _parent(p), HIRNode(HIRNkFuncCallExpr) {}

    std::string
    GetName() const {
        return _name;
    }

    std::vector<HIRNode *> &
    GetArgs() {
        return _args;
    }

    Module *
    GetParentMod() const {
        return _parent;
    }
};

}