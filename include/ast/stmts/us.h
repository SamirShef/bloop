#pragma once
#include <utils/name.h>
#include <ast/stmt.h>

namespace bloop {

class UsingStmt : public Stmt {
    NameObj _path;

public:
    explicit UsingStmt(NameObj p, llvm::SMLoc s, llvm::SMLoc e) : _path(p), Stmt(Pub, NkUsingStmt, s, e) {}

    NameObj
    GetPath() const {
        return _path;
    }
};

}