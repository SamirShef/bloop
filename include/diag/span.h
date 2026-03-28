#pragma once
#include <llvm/Support/SMLoc.h>
#include <string>

namespace bloop {

struct DiagnosticSpan {
    llvm::SMLoc Start;
    llvm::SMLoc End;
    std::string Label;

    explicit DiagnosticSpan(llvm::SMLoc s, llvm::SMLoc e, std::string l) : Start(s), End(e), Label(l) {}
};

}