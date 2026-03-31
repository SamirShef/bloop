#pragma once
#include <lexer/lexer.h>
#include <parser/parser.h>
#include <sema/sema.h>
#include <utils/astPrinter.h>
#include <codegen/codegen.h>
#include <utils/modules/module.h>
#include <utils/compilation.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <filesystem>
#include <iostream>

namespace bloop {

inline std::pair<bool, std::string>
Compile(const std::filesystem::path &filePath, const std::filesystem::path &objPath, Module *mod) {
    llvm::SourceMgr srcMgr;
    DiagnosticEngine diag(srcMgr);

    auto bufferOrErr = llvm::MemoryBuffer::getFile(filePath.string());
    if (std::error_code ec = bufferOrErr.getError()) {
        llvm::errs() << llvm::errs().RED << "File Error (" << filePath.string() << "): " << ec.message() << '\n' << llvm::errs().RESET;
        return { false, "" };
    }
    
    unsigned bufferId = srcMgr.AddNewSourceBuffer(std::move(*bufferOrErr), llvm::SMLoc());
    Lexer lexer(diag, bufferId);
    std::vector<Token> tokens;
    lexer.TokenizeInto(tokens);
    if (diag.GetErrorsCount() > 0) {
        return { false, "" };
    }

    Parser parser(diag, tokens);
    std::vector<Stmt *> ast;
    parser.ParseInto(ast);
    if (diag.GetErrorsCount() > 0) {
        return { false, "" };
    }
    
    Semantic sema(diag, mod);
    sema.Analyze(ast);
    if (diag.GetErrorsCount() > 0) {
        return { false, "" };
    }
    HIRContext context = sema.GetContext();

    /* ASTPrinter printer(srcMgr, std::cout);
    std::cout << '\n';
    printer.Print(ast, Blue); */

    CodeGen codegen(mod->Name, context.GetNodes());
    llvm::Module *llvmMod = codegen.Generate();
    std::cout << '\n';
    /* llvmMod->print(llvm::outs(), nullptr); */

    InitializeLLVMTargets();
    std::string tripleStr = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(tripleStr);

    std::filesystem::create_directories(objPath.parent_path());
    if (!EmitObjectFile(llvmMod, objPath.string(), tripleStr)) {
        return { false, "" };
    }

    return { true, objPath.string() };
}

}