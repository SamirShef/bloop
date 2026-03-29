#pragma once
#include <lexer/token.h>
#include <llvm/Support/SMLoc.h>
#include <string>

namespace bloop {

struct NameObj {
    std::string Name;
    llvm::SMLoc Start;
    llvm::SMLoc End;

    explicit NameObj(std::string n, llvm::SMLoc s, llvm::SMLoc e) : Name(n), Start(s), End(e) {}
    NameObj(const Token &t) : NameObj(t.Val, t.Start, t.End) {}
};

}