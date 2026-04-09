#pragma once
#include <ast/stmt.h>

namespace bloop {

class BreakStmt : public Stmt {
public:
    BreakStmt(llvm::SMLoc s, llvm::SMLoc e) : Stmt(Priv, NkBreakStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkBreakStmt;
    }
};

}