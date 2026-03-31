#pragma once
#include "hir/vds.h"
#include <utils/symbols/function.h>
#include <hir/node.h>

namespace bloop {

struct HIRFuncArgument {
    std::string Name;
    class Type *Type;
    HIRNode    *DefaultVal;

    explicit HIRFuncArgument(std::string n, class Type *t, HIRNode *dv = nullptr) : Name(n), Type(t), DefaultVal(dv) {}
};
    
class HIRFuncDeclStmt : public HIRNode {
    std::string _name;
    Type *_retType;
    std::vector<HIRFuncArgument> _args;
    std::vector<HIRNode *> _body;

public:
    explicit HIRFuncDeclStmt(std::string n, Type *rt, std::vector<HIRFuncArgument> &a)
        : _name(n), _retType(rt), _args(a), HIRNode(HIRNkFuncDeclStmt) {}

    static constexpr bool
    classof(const HIRNode *node) {
        return node->GetKind() == HIRNkFuncDeclStmt;
    }
    
    std::string
    GetName() const {
        return _name;
    }

    Type *
    GetRetType() const {
        return _retType;
    }

    std::vector<HIRFuncArgument> &
    GetArgs() {
        return _args;
    }

    std::vector<HIRNode *> &
    GetBody() {
        return _body;
    }
};

}