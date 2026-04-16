#pragma once
#include <utils/types/type.h>
#include <ast/expr.h>

namespace bloop {

class NilExpr : public Expr {
public:
    explicit NilExpr(llvm::SMLoc s, llvm::SMLoc e) : Expr(NkNilExpr, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkNilExpr;
    }
};

}