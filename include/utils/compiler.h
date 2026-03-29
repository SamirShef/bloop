#pragma once
#include <lexer/lexer.h>
#include <parser/parser.h>
#include <utils/astPrinter.h>
#include <utils/modules/module.h>
#include <llvm/Support/raw_ostream.h>
#include <filesystem>
#include <iostream>

namespace bloop {

static inline void
deleteAST(std::vector<Stmt *> &ast) {
    for (auto &s : ast) {
        s->Delete();
        delete s;
    }
}

inline bool
Compile(const std::filesystem::path &filePath, Module *mod) {
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

    Parser parser(diag, tokens);
    std::vector<Stmt *> ast;
    parser.ParseInto(ast);
    if (diag.GetErrorsCount() > 0) {
        return false;
    }

    ASTPrinter printer(srcMgr, std::cout);
    std::cout << '\n';
    printer.Print(ast, Blue);

    deleteAST(ast);

    return true;
}

}