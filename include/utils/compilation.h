#pragma once
#include <llvm/IR/Module.h>

namespace bloop {

void
InitializeLLVMTargets();

bool
EmitObjectFile(llvm::Module *mod, const std::string &fileName, std::string targetTripleStr);

void
LinkObjectFiles(const std::string &exeFile, std::vector<std::string> objFiles);

std::string
GetOutputName(const std::string &inputFile, const llvm::Triple &triple);

}