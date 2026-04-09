#pragma once
#include <utils/symbols/function.h>
#include <utils/types/type.h>
#include <utils/name.h>
#include <ast/stmt.h>
#include <vector>

namespace bloop {

class FuncDeclStmt : public Stmt {
    NameObj _name;
    std::vector<Argument> _args;
    Type *_retType;
    std::vector<Stmt *> _body;

public:
    explicit FuncDeclStmt(NameObj n, std::vector<Argument> &ar, Type *rt, std::vector<Stmt *> &b, AccessModifier ac, llvm::SMLoc s, llvm::SMLoc e)
        : _name(n), _args(ar), _retType(rt), _body(b), Stmt(ac, NkFuncDeclStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkFuncDeclStmt;
    }
    
    NameObj
    GetName() const {
        return _name;
    }

    std::vector<Argument> &
    GetArgs() {
        return _args;
    }

    Type *&
    GetRetType() {
        return _retType;
    }

    std::vector<Stmt *> &
    GetBody() {
        return _body;
    }
};

}