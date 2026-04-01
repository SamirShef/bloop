#pragma once
#include <llvm/Support/CommandLine.h>

namespace bloop {
    static llvm::cl::OptionCategory Category("Bloop Compiler Options");

    static llvm::cl::SubCommand NewSub("new", "Create a new project");
    static llvm::cl::SubCommand InitSub("init", "Initialize a project in current directory");
    static llvm::cl::SubCommand BuildSub("build", "Build the current project");
    static llvm::cl::SubCommand FetchSub("fetch", "Update the module registry");

    static llvm::cl::opt<std::string> NewName(
        llvm::cl::Positional, llvm::cl::desc("<name>"), 
        llvm::cl::Required, llvm::cl::sub(NewSub)
    );

    static llvm::cl::opt<std::string> InitName(
        llvm::cl::Positional, llvm::cl::desc("[name]"), 
        llvm::cl::Optional, llvm::cl::sub(InitSub)
    );
    
    static llvm::cl::opt<std::string> InputFilename(
        llvm::cl::Positional, llvm::cl::desc("[input file]"), llvm::cl::Optional, llvm::cl::cat(Category)
    );

    enum OptLevel {
        O0,
        O1,
        O2,
        O3
    };

    static llvm::cl::opt<OptLevel> OptimizationLevel(
        llvm::cl::desc("Optimization level:"),
        llvm::cl::values(
            clEnumValN(O0, "O0", "No optimization"),
            clEnumValN(O1, "O1", "Basic optimization"),
            clEnumValN(O2, "O2", "Default optimization"),
            clEnumValN(O3, "O3", "Aggressive optimization")),
        llvm::cl::init(O0), llvm::cl::cat(Category)
    );

    enum ActionKind {
        EmitNone,
        EmitAST,
        EmitLLVM,
    };

    static llvm::cl::opt<ActionKind> EmitAction(
        "emit", llvm::cl::desc("The kind of output to produce:"),
        llvm::cl::values(
            clEnumValN(EmitAST, "ast", "Print the AST to stdout"),
            clEnumValN(EmitLLVM, "llvm", "Emit LLVM IR (.ll)")),
        llvm::cl::init(EmitNone), llvm::cl::cat(Category)
    );
}