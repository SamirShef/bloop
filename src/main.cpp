#include <lexer/lexer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace bloop;

int
main(int argc, char **argv) {
    if (argc != 2) {
        llvm::errs() << llvm::errs().RED << "Usage: bloop <input_file_path>\n" << llvm::errs().RESET;
        return 1;
    }
    std::string fileName = argv[1];
    llvm::SourceMgr srcMgr;
    DiagnosticEngine diag(srcMgr);

    auto bufferOrErr = llvm::MemoryBuffer::getFile(fileName);
    if (std::error_code ec = bufferOrErr.getError()) {
        llvm::errs() << llvm::errs().RED << ec.message() << '\n' << llvm::errs().RESET;
        return 1;
    }
    unsigned bufferId = srcMgr.AddNewSourceBuffer(std::move(*bufferOrErr), llvm::SMLoc());
    Lexer lexer(diag, bufferId);
    std::vector<Token> tokens;
    lexer.TokenizeInto(tokens);
    if (diag.GetErrorsCount()) {
        return 1;
    }
    for (auto t : tokens) {
        llvm::outs() << (uint16_t)t.Kind << '[' << t.Val << "]\n";
    }
    return 0;
}