#pragma once
#include <utils/types/type.h>
#include <hir/node.h>
#include <vector>

namespace bloop {

class HIRStructDeclStmt : public HIRNode {
private:
    std::string _name;
    std::vector<Type *> _fields;

public:
    explicit HIRStructDeclStmt(std::string n, std::vector<Type *> &f) : _name(n), _fields(f), HIRNode(HIRNkStructDeclStmt) {}

    std::string
    GetName() const {
        return _name;
    }

    std::vector<Type *> &
    GetFields() {
        return _fields;
    }
};

}