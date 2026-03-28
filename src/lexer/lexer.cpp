#include <lexer/lexer.h>
#include <lexer/keywords.h>
#include <llvm/Support/SMLoc.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

#define TOK(t, v, s, e) Token(t, v, s, e)
#define LOC1(c) llvm::SMLoc::getFromPointer(c)

namespace bloop {

void
Lexer::TokenizeInto(std::vector<Token> &tokens) {
    while (*_curPtr != '\0') {
        tokens.push_back(nextTok());
    }
}

Token
Lexer::nextTok() {
    while (isspace(*_curPtr)) {
        ++_curPtr;
    }

    if (*_curPtr == '\0') {
        return TOK(TkEof, "", LOC1(_curPtr), LOC1(_curPtr));
    }
    if (isalpha(*_curPtr) || *_curPtr == '_') {
        return tokenizeId(_curPtr);
    }
    if (isdigit(*_curPtr) || *_curPtr == '.' && isdigit(*(_curPtr + 1))) {
        return tokenizeNumLit(_curPtr);
    }
    if (*_curPtr == '\"') {
        return tokenizeStrLit(_curPtr);
    }
    if (*_curPtr == '\'') {
        return tokenizeCharLit(_curPtr);
    }
    if (*_curPtr == '/' && (*(_curPtr + 1) == '/' || *(_curPtr + 1) == '*')) {
        skipComments();
        return nextTok();
    }
    return tokenizeOp(_curPtr);
}

Token
Lexer::tokenizeId(const char *tokStart) {
    while (*_curPtr != '\0' && (isalnum(*_curPtr) || *_curPtr == '_')) {
        ++_curPtr;
    }
    std::string text(tokStart, _curPtr - tokStart);
    if (auto it = keywords.find(text); it != keywords.end()) {
        return TOK(it->second, text, LOC1(tokStart), LOC1(_curPtr));
    }
    return TOK(TkId, text, LOC1(tokStart), LOC1(_curPtr));
}

Token
Lexer::tokenizeNumLit(const char *tokStart) {
    std::string val;
    bool isFirstDigit = true;
    bool hasDot = false;
    enum {
        Bin,
        Oct,
        Dec,
        Hex,
    } kind = Dec;

    #define IS_DEC(c) (c >= '0' && c <= '9')
    #define IS_BIN(c) (c >= '0' && c <= '1')
    #define IS_OCT(c) (c >= '0' && c <= '7')
    #define IS_HEX(c) (c >= '0' && c <= '9' || toupper(c) >= 'A' && toupper(c) <= 'F')
    #define IS_DIGIT(c) (kind == Dec ? IS_DEC(c) : \
                            (kind == Bin ? IS_BIN(c) : \
                            (kind == Oct ? IS_OCT(c) : \
                            (kind == Hex ? IS_HEX(c) : false))))
    
    while (*_curPtr != '\0' || *_curPtr == '_' || *_curPtr == '.') {
        if (*_curPtr == '_') {
            ++_curPtr;
            continue;
        }
        if (*_curPtr == '.' && isFirstDigit) {
            ++_curPtr;
            hasDot = true;
            val += "0.";
            continue;
        }
        if (isFirstDigit && *_curPtr == '0') {
            _curPtr += 2;
            switch (*(_curPtr - 1)) {
                case 'b':
                case 'B':
                    kind = Bin;
                    break;
                case 'o':
                case 'O':
                    kind = Oct;
                    break;
                case 'x':
                case 'X':
                    kind = Hex;
                    break;
                default:
                    _curPtr -= 2;
            }
        }
        if (IS_DIGIT(*_curPtr) || *_curPtr == '.') {
            if (*_curPtr == '.') {
                if (hasDot || kind != Dec) {
                    break;
                }
                else {
                    hasDot = true;
                }
            }
            val += std::string { *_curPtr++ };
        }
        else {
            break;
        }
        isFirstDigit = false;
    }

    #undef IS_DIGIT
    #undef IS_HEX
    #undef IS_OCT
    #undef IS_BIN
    #undef IS_DEC

    /*
     * 00000000
     * ^ ^^^^^
     * | |||||is 32-bit ingeter (empty)
     * | |||||
     * | ||||is 64-bit ingeter  (`L` or `l`)
     * | ||||
     * | |||is 128-bit integer  (`B` or `b`)
     * | |||
     * | ||is size type         (`S` or `s`)
     * | ||
     * | |is float              (`F` or `f`)
     * | |
     * | is double              (empty)
     * |
     * is unsigned bit          (`U` or `u`)
     */
    char suffixMask = 0;
    changeSuffixMask(suffixMask);
    ++_curPtr;
    if (hasDot && suffixMask <= 1) {
        suffixMask = 1 << 5;
    }
    bool isUnsigned = (suffixMask & (1 << 7)) >> 7;
    int i = 0;
    for (; i < 7 && !((suffixMask & (1 << i)) >> i); ++i);
    if (hasDot && i <= 3) {
        i = 5; // f64
    }
    TokenKind lit = !hasDot ? (TokenKind)(TkI32Lit + i) : (TokenKind)(TkF32Lit + i - 4);
    return TOK((TokenKind)(lit + isUnsigned * 5), val, LOC1(tokStart), LOC1(_curPtr));
}

Token
Lexer::tokenizeStrLit(const char *tokStart) {
    ++_curPtr;  // skip "
    std::string val;
    while (*_curPtr != '\0' && *_curPtr != '\"') {
        char c;
        if (*_curPtr == '\\') {
            val += getEscapeSecuence(++_curPtr);
            continue;
        }
        else {
            c = *(_curPtr++);
        }
        val += c;
    }
    ++_curPtr;  // skip "
    return TOK(TkStrLit, val, LOC1(tokStart), LOC1(_curPtr));
}

Token
Lexer::tokenizeCharLit(const char *tokStart) {
    ++_curPtr;  // skip '
    std::string val;

    if (*_curPtr == '\'') {
        ++_curPtr;
        // TODO: create compilation error
        return TOK(TkUnknown, "", LOC1(tokStart), LOC1(_curPtr));
    }

    if (*_curPtr == '\\') {
        val += getEscapeSecuence(++_curPtr);
    }
    else {
        unsigned char lead = static_cast<unsigned char>(*_curPtr);
        int len = 0;

        if (lead < 0x80) {
            len = 1;    // 0xxxxxxx (ASCII)
        }
        else if ((lead & 0xE0) == 0xC0) {
            len = 2;    // 110xxxxx
        }
        else if ((lead & 0xF0) == 0xE0) {
            len = 3;    // 1110xxxx
        }
        else if ((lead & 0xF8) == 0xF0) {
            len = 4;    // 11110xxx
        }
        else {
            len = 1;
        }

        for (int i = 0; i < len && *_curPtr != '\0' && *_curPtr != '\''; ++i) {
            val += *_curPtr++;
        }
    }

    if (*_curPtr == '\'') {
        ++_curPtr;  // skip '
    }
    else {
        // TODO: create compilation error
    }

    return TOK(TkCharLit, val, LOC1(tokStart), LOC1(_curPtr));
}

Token
Lexer::tokenizeOp(const char *tokStart) {
    switch (*(_curPtr++)) {
        #define TOKEN(t) TOK(t, std::string(tokStart, _curPtr - tokStart), LOC1(tokStart), LOC1(_curPtr))
        #define CASE1(e, t) case e: return TOKEN(t);
        #define CASE2(e) case e:
        #define IS(e, t) if (*_curPtr == (e)) { \
                ++_curPtr; \
                return TOKEN(t); \
            }
        #define IS_EQ(t) IS('=', t)
        
        CASE1(';', TkSemi)
        CASE1(',', TkComma)
        CASE1('.', TkDot)
        CASE1('(', TkLParen)
        CASE1(')', TkRParen)
        CASE1('{', TkLBrace)
        CASE1('}', TkRBrace)
        CASE1('[', TkLBracket)
        CASE1(']', TkRBracket)
        CASE1('~', TkTilde)
        CASE1('?', TkQuestion)
        CASE1(':', TkColon)
        CASE2('=')
            IS_EQ(TkEqEq)
            return TOKEN(TkEq);
        CASE2('!')
            IS_EQ(TkNotEq)
            return TOKEN(TkBang);
        CASE2('<')
            IS_EQ(TkLtEq)
            else if (*_curPtr == '<') {
                ++_curPtr;
                IS_EQ(TkLtLtEq)
                return TOKEN(TkLtLt);
            }
            return TOKEN(TkLt);
        CASE2('>')
            IS_EQ(TkGtEq)
            else if (*_curPtr == '>') {
                ++_curPtr;
                IS_EQ(TkGtGtEq)
                return TOKEN(TkGtGt);
            }
            return TOKEN(TkGt);
        CASE2('&')
            IS('&', TkLogAnd)
            IS_EQ(TkAndEq)
            return TOKEN(TkAnd);
        CASE2('|')
            if (*_curPtr == '|') {
                ++_curPtr;
                return TOKEN(TkLogOr);
            }
            IS_EQ(TkOrEq);
            return TOKEN(TkOr);
        CASE2('+')
            IS_EQ(TkPlusEq);
            return TOKEN(TkPlus);
        CASE2('-')
            IS_EQ(TkMinusEq);
            return TOKEN(TkMinus);
        CASE2('*')
            IS_EQ(TkStarEq)
            return TOKEN(TkStar);
        CASE2('/')
            IS_EQ(TkSlashEq)
            return TOKEN(TkSlash);
        CASE2('%')
            IS_EQ(TkPercentEq)
            return TOKEN(TkPercent);
        CASE1('^', TkCarret)
        default:
            return TOKEN(TkUnknown);

        #undef IS_EQ
        #undef IS
        #undef CASE2
        #undef CASE1
        #undef TOKEN
    }
}

char
Lexer::changeSuffixMask(char &mask) {
    switch (toupper(*_curPtr)) {
        case 'U': {
            mask |= 1 << 7;
            ++_curPtr;
            char newMask = changeSuffixMask(mask);
            return newMask;
        }
        case 'L':
            mask |= 1 << 1;
            break;
        case 'B':
            mask |= 1 << 2;
            break;
        case 'S':
            mask |= 1 << 3;
            break;
        case 'F':
            mask |= 1 << 4;
            break;
        default:
            mask |= 1;
            --_curPtr;
            break;
    }
    return mask;
}

void
Lexer::skipComments() {
    _curPtr += 2;
    bool isMultilineComment = *(_curPtr - 1) == '*';
    if (isMultilineComment) {
        while (*_curPtr != '\0' && (*_curPtr != '*' || *(_curPtr + 1) != '/')) {
            ++_curPtr;
        }
        _curPtr += 2;
    }
    else {
        while (*_curPtr != '\0' && *_curPtr != '\n') {
            ++_curPtr;
        }
    }
}

std::string 
toUtf8(uint32_t cp) {
    std::string result;
    if (cp <= 0x7F) {
        result += static_cast<char>(cp);
    }
    else if (cp <= 0x7FF) {
        result += static_cast<char>(0xC0 | (cp >> 6));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (cp >> 12));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF) {
        result += static_cast<char>(0xF0 | (cp >> 18));
        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return result;
}

std::string
Lexer::getEscapeSecuence(const char *tokStart) {
    switch (*(_curPtr++)) {
        #define CASE(c, e) case c: return { e };
        CASE('a', '\a')
        CASE('b', '\b')
        CASE('e', '\e')
        CASE('f', '\f')
        CASE('r', '\r')
        CASE('n', '\n')
        CASE('t', '\t')
        CASE('v', '\v')
        CASE('\\', '\\')
        CASE('\'', '\'')
        CASE('\"', '\"')
        CASE('\?', '\?')
        CASE('0', '\0')
        case 'u': { // \uXXXX (16-bit)
            uint32_t cp = 0;
            for (int i = 0; i < 4; ++i) {
                char c = *_curPtr++;
                cp <<= 4;
                if (isdigit(c)) {
                    cp |= (c - '0');
                }
                else if ((unsigned)(toupper(c) - 'A') < 6) {
                    cp |= toupper(c) - 'A' + 10;
                }
            }
            return toUtf8(cp);
        }
        case 'U': { // \UXXXXXXXX (32-bit)
            uint32_t cp = 0;
            for (int i = 0; i < 8; ++i) {
                char c = *_curPtr++;
                cp <<= 4;
                if (isdigit(c)) {
                    cp |= (c - '0');
                }
                else if ((unsigned)(toupper(c) - 'A') < 6) {
                    cp |= toupper(c) - 'A' + 10;
                }
            }
            return toUtf8(cp);
        }
        default:
            --_curPtr;
            // TODO: create an compilation error
            return { *tokStart };
    }
}

}

#undef LOC1
#undef TOK