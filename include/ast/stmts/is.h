#pragma once
#include <ast/stmts/fds.h>
#include <utils/types/type.h>
#include <ast/stmt.h>
#include <vector>

namespace bloop {

class ImplStmt : public Stmt {
public:
    struct Method {
        NameObj Name;
        std::vector<Argument> Args;
        Type *RetType;
        std::vector<Stmt *> Body;
        AccessModifier Access;
        bool IsStatic;
    };

private:
    Type *_structType;
    Type *_traitType;
    std::vector<Method> _methods;

public:
    explicit ImplStmt(Type *st, Type *tt, std::vector<Method> &m, llvm::SMLoc s, llvm::SMLoc e)
        : _structType(st), _traitType(tt), _methods(m), Stmt(Priv, NkImplStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkImplStmt;
    }

    Type *&
    GetStructType() {
        return _structType;
    }

    Type *&
    GetTraitType() {
        return _traitType;
    }

    std::vector<Method> &
    GetMethods() {
        return _methods;
    }
};

}