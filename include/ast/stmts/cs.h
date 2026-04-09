#pragma once
#include <ast/stmt.h>

namespace bloop {

class ContinueStmt : public Stmt {
public:
    ContinueStmt(llvm::SMLoc s, llvm::SMLoc e) : Stmt(Priv, NkContinueStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkContinueStmt;
    }
};

}