#pragma once
#include <lexer/token.h>
#include <llvm/Support/SourceMgr.h>
#include <vector>

namespace bloop {

class Lexer {
    llvm::SourceMgr &_srcMgr;
    const char *_curPtr;

public:
    explicit Lexer(llvm::SourceMgr &mgr, unsigned bufferId) : _srcMgr(mgr) {
        auto *buf = _srcMgr.getMemoryBuffer(bufferId);
        _curPtr = buf->getBufferStart();
    }

    void
    TokenizeInto(std::vector<Token> &tokens);

private:
    Token
    nextTok();

    Token
    tokenizeId(const char *tokStart);

    Token
    tokenizeNumLit(const char *tokStart);

    Token
    tokenizeStrLit(const char *tokStart);

    Token
    tokenizeCharLit(const char *tokStart);

    Token
    tokenizeOp(const char *tokStart);

    char
    changeSuffixMask(char &mask);

    void
    skipComments();

    std::string
    getEscapeSecuence(const char *tokStart);

    // TODO: add tokenizing of UTF-8 characters in literals
};

}