#pragma once
#include <utils/modules/module.h>
#include <lexer/lexer.h>
#include <llvm/Support/raw_ostream.h>
#include <filesystem>

namespace bloop {

inline bool
compile(const std::filesystem::path &filePath, Module *mod) {
    llvm::SourceMgr srcMgr;
    DiagnosticEngine diag(srcMgr);

    auto bufferOrErr = llvm::MemoryBuffer::getFile(filePath.string());
    if (std::error_code ec = bufferOrErr.getError()) {
        llvm::errs() << llvm::errs().RED << "File Error (" << filePath.string() << "): " << ec.message() << '\n' << llvm::errs().RESET;
        return false;
    }
    
    unsigned bufferId = srcMgr.AddNewSourceBuffer(std::move(*bufferOrErr), llvm::SMLoc());
    Lexer lexer(diag, bufferId);
    std::vector<Token> tokens;
    
    lexer.TokenizeInto(tokens);
    if (diag.GetErrorsCount() > 0) {
        return false;
    }

    /* llvm::outs() << "Tokens for [" << mod->Name << "]:\n";
    for (auto t : tokens) {
        llvm::outs() << "  " << (uint16_t)t.Kind << " [" << t.Val << "]\n";
    } */

    return true;
}

}