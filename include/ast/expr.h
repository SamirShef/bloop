#pragma once
#include <ast/node.h>

namespace bloop {

class Expr : public Node {
public:
    explicit Expr(NodeKind k, llvm::SMLoc s, llvm::SMLoc e) : Node(k, s, e) {}

    virtual void
    Delete() = 0;

    static constexpr bool
    classof(const Node *n) {
        return n->GetKind() > NkStartExprs && n->GetKind() < NkEndExprs;
    }
};

}