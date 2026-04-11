#pragma once
#include <utils/name.h>
#include <ast/stmt.h>
#include <vector>

namespace bloop {

class UsingStmt : public Stmt {
    std::vector<NameObj> _path;

public:
    explicit UsingStmt(std::vector<NameObj> &p, llvm::SMLoc s, llvm::SMLoc e) : _path(p), Stmt(Pub, NkUsingStmt, s, e) {}

    constexpr static bool
    classof(const Node *node) {
        return node->GetKind() == NkUsingStmt;
    }
    
    std::vector<NameObj> &
    GetPath() {
        return _path;
    }
};

}