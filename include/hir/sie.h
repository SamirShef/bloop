#pragma once
#include <hir/node.h>
#include <string>
#include <vector>

namespace bloop {

class HIRStructInstanceExpr : public HIRNode {
    std::string _name;
    std::vector<std::pair<int, HIRNode *>> _fields;

public:
    explicit HIRStructInstanceExpr(std::string n, std::vector<std::pair<int, HIRNode *>> &a) : _name(n), _fields(a), HIRNode(HIRNkStructInstanceExpr) {}

    std::string
    GetName() const {
        return _name;
    }

    std::vector<std::pair<int, HIRNode *>> &
    GetFields() {
        return _fields;
    }
};

}