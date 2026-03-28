#pragma once
#include "diag/engine.h"
#include <lexer/token.h>
#include <llvm/Support/SourceMgr.h>
#include <vector>

namespace bloop {

class Lexer {
    DiagnosticEngine &_diag;
    llvm::SourceMgr &_srcMgr;
    const char *_curPtr;

public:
    explicit Lexer(DiagnosticEngine &diag, unsigned bufferId) : _diag(diag), _srcMgr(diag.GetSourceMgr()) {
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
    toUtf8(uint32_t cp);
    
    std::string
    getEscapeSecuence(const char *tokStart);
};

}