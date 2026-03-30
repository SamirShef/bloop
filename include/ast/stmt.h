#pragma once
#include <utils/symbols/access.h>
#include <ast/node.h>

namespace bloop {

class Stmt : public Node {
    AccessModifier _access;
    
public:
    explicit Stmt(AccessModifier a, NodeKind k, llvm::SMLoc s, llvm::SMLoc e) : _access(a), Node(k, s, e) {}

    AccessModifier
    GetAccess() const {
        return _access;
    }

    static constexpr bool
    classof(const Node *n) {
        return n->GetKind() > NkStartStmts && n->GetKind() < NkEndStmts;
    }
};

}