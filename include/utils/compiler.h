#pragma once
#include <lexer/lexer.h>
#include <parser/parser.h>
#include <sema/sema.h>
#include <codegen/codegen.h>
#include <utils/astPrinter.h>
#include <utils/options.h>
#include <utils/modules/module.h>
#include <utils/compilation.h>
#include <utils/bitcode/serializer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <filesystem>
#include <iostream>

namespace bloop {

inline std::pair<bool, std::string>
Compile(const std::unordered_map<std::string, FileNode> &graph, const std::filesystem::path &rootPath, const std::filesystem::path &filePath, const std::filesystem::path &objPath, Module *mod) {
    fs::create_directories(objPath.parent_path());
    llvm::SourceMgr srcMgr;
    DiagnosticEngine diag(srcMgr);

    auto bufferOrErr = llvm::MemoryBuffer::getFile(filePath.string());
    if (std::error_code ec = bufferOrErr.getError()) {
        llvm::errs() << llvm::errs().RED << "File Error (" << filePath.lexically_relative(rootPath).lexically_normal().string() << "): " << ec.message() << '\n' << llvm::errs().RESET;
        return { false, "" };
    }
    
    unsigned bufferId = srcMgr.AddNewSourceBuffer(std::move(*bufferOrErr), llvm::SMLoc());
    Lexer lexer(&diag, bufferId);
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
    
    Semantic sema(diag, mod, graph);
    sema.Analyze(ast);
    if (diag.GetErrorsCount() > 0) {
        return { false, "" };
    }
    HIRContext context = sema.GetContext();

    Serializer serializer;
    std::filesystem::path bitcodeFile = objPath;
    bitcodeFile.replace_extension(".blmod");
    serializer.Serialize(mod, bitcodeFile.string());

    if (EmitAction == EmitAST) {
        ASTPrinter printer(srcMgr, std::cout);
        std::cout << '\n';
        printer.Print(ast, Blue);
    }

    CodeGen codegen(mod->Name, context.GetNodes());
    llvm::Module *llvmMod = codegen.Generate();

    InitializeLLVMTargets();
    std::string tripleStr = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(tripleStr);

    if (!EmitObjectFile(llvmMod, objPath.string(), tripleStr)) {
        return { false, "" };
    }

    if (EmitAction == EmitLLVM) {
        std::error_code ec;
        std::filesystem::path llvmIRPath = objPath;
        llvm::raw_fd_ostream os(llvmIRPath.replace_extension(".ll").string(), ec);
        if (ec) {
            llvm::errs() << llvm::errs().RED << ec.message() << llvm::errs().RESET;
            exit(1);
        }
        llvmMod->print(os, nullptr);
        std::cout << "     [Info] LLVM IR emitted to: " << llvmIRPath.string() << '\n';
    }

    return { true, objPath.string() };
}

}