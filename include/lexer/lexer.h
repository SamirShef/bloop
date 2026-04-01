#pragma once
#include "diag/engine.h"
#include <lexer/token.h>
#include <llvm/Support/SourceMgr.h>
#include <vector>

namespace bloop {

class Lexer {
    DiagnosticEngine *_diag = nullptr;
    llvm::SourceMgr *_srcMgr = nullptr;
    const char *_curPtr = nullptr;

public:
    explicit Lexer() {}

    explicit Lexer(DiagnosticEngine *diag, unsigned bufferId) : _diag(diag), _srcMgr(diag->GetSourceMgr()) {
        auto *buf = _srcMgr->getMemoryBuffer(bufferId);
        _curPtr = buf->getBufferStart();
    }

    void
    TokenizeInto(std::vector<Token> &tokens) {
        while (*_curPtr != '\0') {
            tokens.push_back(nextTok());
        }
    }

    std::vector<std::string>
    PeekDependencies(std::string buffer) {
        _curPtr = &buffer[0];
        std::vector<Token> tokens;
        TokenizeInto(tokens);

        std::vector<std::string> dependencies;

        for (int i = 0; i < tokens.size(); ++i) {
            if (tokens[i].Kind == TkUsing) {
                ++i;
                std::string dependence;
                while (tokens[i].Kind != TkSemi && tokens[i].Kind != TkEof && tokens[i].Kind != TkUnknown) {
                    dependence += tokens[i].Val;
                    ++i;
                }
                dependencies.push_back(dependence);
            }
        }
        
        return dependencies;
    }

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
    toUtf8(uint32_t cp) const;

    std::string
    getEscapeSecuence(const char *tokStart);
};

}