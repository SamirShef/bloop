#include <utils/types/types.h>
#include <parser/parser.h>
#include <diag/level.h>
#include <ast/ast.h>

static bloop::AccessModifier access;

namespace bloop {

#define EXPECT_SEMI() \
    if (consumeSemi && !expect(TkSemi)) { \
        _diag.Report(Error, "expected ';'") \
            .SetCode(ErrExpectedSemi) \
            .AddSpan(peek().Start, peek().End) \
            .AddNote("add ';' to the end of the line"); \
    }

Stmt *
Parser::parseStmt(bool consumeSemi) {
    if (peek().Kind == TkEof) {
        return nullptr;
    }

    access = expect(TkPub) ? Pub : Priv;
    switch (peek().Kind) {
        case TkVar:
        case TkConst: {
            Stmt *res = parseVDS();
            EXPECT_SEMI();
            return res;
        }
        case TkFunc: {
            return parseFDS();
        }
        case TkUsing: {
            Stmt *res = parseUS();
            EXPECT_SEMI();
            return res;
        }
    }
    const Token tok = advance();
    _diag.Report(Error, "expected statement")
        .SetCode(ErrExpectedStmt)
        .AddSpan(tok.Start, tok.End);
    return nullptr;
}

Stmt *
Parser::parseVDS() {
    const Token firstTok = advance();
    const Token nameTok = advance();
    if (nameTok.Kind != TkId) {
        _diag.Report(Error, "expected identifier")
            .SetCode(ErrExpectedId)
            .AddSpan(nameTok.Start, nameTok.End);
    }
    NameObj name = nameTok;
    Type *type = nullptr;
    if (expect(TkColon)) {
        type = consumeType();
    }
    Expr *expr = nullptr;
    if (expect(TkEq)) {
        expr = parseExpr();
    }
    return new VarDeclStmt(name, type, expr, firstTok.Kind == TkConst, access, firstTok.Start, peek().End);
}

Stmt *
Parser::parseFDS() {
    AccessModifier accessCopy = access;
    const Token firstTok = advance();
    const Token nameTok = advance();
    if (nameTok.Kind != TkId) {
        _diag.Report(Error, "expected identifier")
            .SetCode(ErrExpectedId)
            .AddSpan(nameTok.Start, nameTok.End);
    }
    NameObj name = nameTok;
    if (!expect(TkLParen)) {
        _diag.Report(Error, "expected '('")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    std::vector<Argument> args;
    while (!expect(TkRParen)) {
        args.push_back(parseArgument());
        if (peek().Kind != TkRParen) {
            if (!expect(TkComma)) {
                _diag.Report(Error, "expected ','")
                    .SetCode(ErrExpectedToken)
                    .AddSpan(peek().Start, peek().End);
            }
        }
    }
    Type *retType = nullptr;
    if (expect(TkColon)) {
        retType = consumeType();
    }

    if (!expect(TkLBrace)) {
        _diag.Report(Error, "expected '{'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    std::vector<Stmt *> body;
    while (!expect(TkRBrace)) {
        body.push_back(parseStmt());
    }

    return new FuncDeclStmt(name, args, retType, body, accessCopy, firstTok.Start, peek().End);
}

Stmt *
Parser::parseUS() {
    const Token firstTok = advance();
    NameObj path("", peek().Start, peek().End);
    while (peek().Kind != TkSemi) {
        if (peek().Kind == TkId || peek().Kind == TkDot) {
            path.Name += advance().Val;
        }
        else {
            _diag.Report(Error, "expected identifier of '.'")
                .SetCode(ErrExpectedToken)
                .AddSpan(peek().Start, peek().End);
            break;
        }
    }
    path.End = peek(-1).End;
    return new UsingStmt(path, firstTok.Start, peek().End);
}

Expr *
Parser::parsePrefixExpr(bool allowStruct) {
    const Token tok = advance();
    switch (tok.Kind) {
        #define LIT(t, v) new LiteralExpr(Value(Value::Const, ValueData(v), new t, tok.Start, tok.End))
        #define INT_LIT(bw, iu) LIT(IntegerType(bw, iu, tok.Start, tok.End), (int64_t)stoll(tok.Val))
        #define FLOAT_LIT(t) LIT(FloatingType(FloatingType::t, tok.Start, tok.End), (double)stold(tok.Val))
        #define SIZE_LIT(iu) LIT(SizeType(iu, tok.Start, tok.End), (int64_t)stoll(tok.Val))

        case TkBoolLit:
            return INT_LIT(1, false);
        case TkCharLit:
            return LIT(CharType(tok.Start, tok.End), tok.Val);
        case TkI16Lit:
            return INT_LIT(16, false);
        case TkI32Lit:
            return INT_LIT(32, false);
        case TkI64Lit:
            return INT_LIT(64, false);
        case TkI128Lit:
            return INT_LIT(128, false);
        case TkISizeLit:
            return SIZE_LIT(false);
        case TkU16Lit:
            return INT_LIT(16, true);
        case TkU32Lit:
            return INT_LIT(32, true);
        case TkU64Lit:
            return INT_LIT(64, true);
        case TkU128Lit:
            return INT_LIT(128, true);
        case TkUSizeLit:
            return SIZE_LIT(true);
        case TkF32Lit:
            return FLOAT_LIT(Float);
        case TkF64Lit:
            return FLOAT_LIT(Double);
        case TkStrLit:
            return LIT(StringType(tok.Start, tok.End), tok.Val);
        
        #undef SIZE_LIT
        #undef FLOAT_LIT
        #undef INT_LIT
        #undef LIT
        
        case TkMinus:
        case TkBang: {
            return new UnaryExpr(tok, parsePrefixExpr(allowStruct), tok.Start, peek().End);
        }

        case TkId:
            return new VarExpr(tok);
    }
    _diag.Report(Error, "expected expression")
        .SetCode(ErrExpectedExpr)
        .AddSpan(tok.Start, tok.End);
    return nullptr;
}

Expr *
Parser::parseExpr(int minPrec, bool allowStruct) {
    Expr *lhs = parsePrefixExpr(allowStruct);
    if (!lhs) {
        return nullptr;
    }

    int prec;
    while (minPrec < (prec = GetTokPrecedence(peek().Kind))) {
        const Token op = advance();

        Expr *rhs = parseExpr(prec, allowStruct);
        lhs = new BinaryExpr(lhs, op, rhs, lhs->GetStartLoc(), peek().End);
    }

    return lhs;
}

Argument
Parser::parseArgument() {
    const Token nameTok = advance();
    if (nameTok.Kind != TkId) {
        _diag.Report(Error, "expected identifier")
            .SetCode(ErrExpectedId)
            .AddSpan(nameTok.Start, nameTok.End);
    }
    NameObj name = nameTok;
    
    if (!expect(TkColon)) {
        _diag.Report(Error, "expected ':'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    Type *type = consumeType();
    Expr *defaultVal = nullptr;
    if (expect(TkEq)) {
        defaultVal = parseExpr();
    }
    return Argument(name, type, defaultVal);
}

Type *
Parser::consumeType() {
    const Token c = advance();
    switch (c.Kind) {
        case TkChar:
            return new CharType(c.Start, c.End);
        case TkBool:
        case TkI8:
        case TkI16:
        case TkI32:
        case TkI64:
        case TkI128:
        case TkU8:
        case TkU16:
        case TkU32:
        case TkU64:
        case TkU128: {
            if (c.Kind == TkBool) {
                return new IntegerType(1, false, c.Start, c.End);
            }
            bool isUnsigned = c.Kind >= TkU8;
            unsigned bitWidth = 1 << ((isUnsigned ? c.Kind - TkU8 : c.Kind - TkI8) + 3);
            return new IntegerType(bitWidth, isUnsigned, c.Start, c.End);
        }
        case TkISize:
        case TkUSize:
            return new SizeType(c.Kind == TkUSize, c.Start, c.End);
        case TkF32:
        case TkF64:
            return new FloatingType((FloatingType::FloatingKind)(c.Kind - TkF32), c.Start, c.End);
        case TkString:
            return new StringType(c.Start, c.End);
        case TkNoth:
            return new NothType(c.Start, c.End);
        case TkId: {
            return new UnknownNamedType(c, c.Start, c.End);
        }
        case TkFunc:
            // TODO: implement
        case TkStar: {
            Type *base = consumeType();
            return new PointerType(base, c.Start, base->GetEndLoc());
        }
        case TkLBracket: {
            Type *base = consumeType();
            Expr *size = nullptr;
            if (expect(TkComma)) {
                size = parseExpr();
            }
            if (!expect(TkRBracket)) {
                _diag.Report(Error, "expected ']'")
                    .SetCode(ErrExpectedToken)
                    .AddSpan(peek().Start, peek().End);
            }
            return new ArrayType(base, size, c.Start, base->GetEndLoc());
        }
        case TkLParen: {
            std::vector<Type *> types;
            while (!expect(TkRParen)) {
                types.push_back(consumeType());
                if (peek().Kind != TkRParen) {
                    if (!expect(TkComma)) {
                        _diag.Report(Error, "expected ','")
                            .SetCode(ErrExpectedToken)
                            .AddSpan(c.Start, c.End);
                    }
                }
            }
            return new TupleType(types, c.Start, peek(-1).End);
            //                                                 ^^ because ')' was skipped
        }
    }
    _diag.Report(Error, "expected type")
        .SetCode(ErrExpectedType)
        .AddSpan(c.Start, c.End);
    return nullptr;
}

const Token
Parser::peek(uint rpos) {
    if (pos + rpos < _tokens.size()) {
        return _tokens[pos + rpos];
    }
    if (_tokens.size()) {
        auto &srcMgr = _diag.GetSourceMgr();
        unsigned bufferId = srcMgr.FindBufferContainingLoc(_tokens[0].Start);
        const char *end = srcMgr.getBufferInfo(bufferId).Buffer->getBufferEnd();
        return Token(TkEof, "", llvm::SMLoc::getFromPointer(end), llvm::SMLoc::getFromPointer(end));
    }
    return Token(TkEof, "", llvm::SMLoc(), llvm::SMLoc());
}

const Token
Parser::advance() {
    return peek(-pos + pos++);
}

bool
Parser::expect(TokenKind kind) {
    if (peek().Kind == kind) {
        advance();
        return true;
    }
    return false;
}

}