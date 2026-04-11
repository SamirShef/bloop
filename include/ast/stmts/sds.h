#pragma once
#include <ast/expr.h>
#include <utils/types/type.h>
#include <utils/name.h>
#include <ast/stmt.h>
#include <vector>

namespace bloop {

class StructDeclStmt : public Stmt {
public:
    struct Field {
        NameObj         Name;
        class Type     *Type;
        AccessModifier  Access;
        bool            IsStatic;
        Expr           *DefaultVal;
    };

private:
    NameObj _name;
    std::vector<Field> _fields;

public:
    explicit StructDeclStmt(NameObj n, std::vector<Field> &f, AccessModifier a, llvm::SMLoc s, llvm::SMLoc e)
        : _name(n), _fields(f), Stmt(a, NkStructDeclStmt, s, e) {}

    static bool
    classof(const Node *node) {
        return node->GetKind() == NkStructDeclStmt;
    }

    NameObj
    GetName() const {
        return _name;
    }

    std::vector<Field> &
    GetFields() {
        return _fields;
    }
};

}