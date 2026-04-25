#include <utils/types/types.h>
#include <parser/parser.h>
#include <diag/level.h>
#include <ast/ast.h>

static bloop::AccessModifier access;

namespace bloop {

static bool
isAssignmentOp(TokenKind kind) {
    return kind >= TkPlusEq && kind <= TkGtGtEq || kind == TkEq;
}

#define EXPECT_SEMI() \
    if (consumeSemi && !expect(TkSemi)) { \
        _diag.Report(Error, "expected ';'") \
            .SetCode(ErrExpectedSemi) \
            .AddSpan(peek().Start, peek().End) \
            .AddHelp("add ';' to the end of the line"); \
    }

Stmt *
Parser::parseStmt(bool consumeSemi) {
    if (peek().Kind == TkEof) {
        ++_pos;
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
        case TkRet: {
            Stmt *res = parseRS();
            EXPECT_SEMI();
            return res;
        }
        case TkIf: {
            return parseIES();
        }
        case TkFor: {
            return parseFLS();
        }
        case TkBreak: {
            const Token firstTok = advance();
            Stmt *res = createNode<BreakStmt>(firstTok.Start, peek().End);
            EXPECT_SEMI();
            return res;
        }
        case TkContinue: {
            const Token firstTok = advance();
            Stmt *res = createNode<ContinueStmt>(firstTok.Start, peek().End);
            EXPECT_SEMI();
            return res;
        }
        case TkId:
        case TkStar:
        case TkLParen: {
            Expr *expr = parseExpr();
            return getStmtFromExpr(expr, consumeSemi);
        }
        case TkStruct: {
            return parseSDS();
        }
        case TkImpl: {
            return parseIS();
        }
        case TkDel: {
            Stmt *res = parseDS();
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
Parser::getStmtFromExpr(Expr *expr, bool consumeSemi) {
    switch (expr->GetKind()) {
        case NkVarExpr: {
            Stmt *res = parseVAS(llvm::cast<VarExpr>(expr));
            EXPECT_SEMI();
            return res;
        }
        case NkFieldExpr: {
            Stmt *res = parseFAS(llvm::cast<FieldExpr>(expr));
            EXPECT_SEMI();
            return res;
        }
        case NkDerefExpr: {
            Stmt *res = parseDAS(llvm::cast<DerefExpr>(expr));
            EXPECT_SEMI();
            return res;
        }
        case NkArrayAccessExpr: {
            Stmt *res = parseAAS(llvm::cast<ArrayAccessExpr>(expr));
            EXPECT_SEMI();
            return res;
        }
        case NkFuncCallExpr: {
            FuncCallExpr *fce = llvm::cast<FuncCallExpr>(expr);
            Stmt *res = createNode<FuncCallStmt>(fce);
            EXPECT_SEMI();
            return res;
        }
        case NkMethodCallExpr: {
            MethodCallExpr *mce = llvm::cast<MethodCallExpr>(expr);
            Stmt *res = createNode<MethodCallStmt>(mce);
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
    return createNode<VarDeclStmt>(name, type, expr, firstTok.Kind == TkConst, access, firstTok.Start, peek().End);
}

Stmt *
Parser::parseVAS(VarExpr *base) {
    if (!isAssignmentOp(peek().Kind)) {
        _diag.Report(Error, "expected `=` or `+=` or `-=` or `*=` or `/=` or `%=`")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
        return nullptr;
    }
    const Token op = advance();
    Expr *expr = parseExpr();
    if (op.Kind != TkEq && isAssignmentOp(op.Kind)) {
        expr = createCompoundAssignmentOp(op, base, expr);
    }
    NameObj name = base->GetName();
    return createNode<VarAsgnStmt>(name, expr, name.Start, peek().End);
}

Stmt *
Parser::parseFAS(FieldExpr *base) {
    if (!isAssignmentOp(peek().Kind)) {
        _diag.Report(Error, "expected `=` or `+=` or `-=` or `*=` or `/=` or `%=`")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
        return nullptr;
    }
    const Token op = advance();
    Expr *expr = parseExpr();
    if (op.Kind != TkEq && isAssignmentOp(op.Kind)) {
        expr = createCompoundAssignmentOp(op, base, expr);
    }
    return createNode<FieldAsgnStmt>(base->GetBase(), base->GetName(), expr, peek().End);
}

Stmt *
Parser::parseDAS(DerefExpr *base) {
    if (!isAssignmentOp(peek().Kind)) {
        _diag.Report(Error, "expected '=' or compound assignment")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
        return nullptr;
    }
    const Token op = advance();
    Expr *expr = parseExpr();
    if (op.Kind != TkEq && isAssignmentOp(op.Kind)) {
        expr = createCompoundAssignmentOp(op, base, expr);
    }
    return createNode<DerefAsgnStmt>(base, expr, base->GetStartLoc(), peek().End);
}

Stmt *
Parser::parseAAS(ArrayAccessExpr *base) {
    if (!isAssignmentOp(peek().Kind)) {
        _diag.Report(Error, "expected `=` or `+=` or `-=` or `*=` or `/=` or `%=`")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
        return nullptr;
    }
    const Token op = advance();
    Expr *expr = parseExpr();
    if (op.Kind != TkEq && isAssignmentOp(op.Kind)) {
        expr = createCompoundAssignmentOp(op, base, expr);
    }
    return createNode<ArrayAsgnStmt>(base->GetBase(), base->GetIndex(), expr, peek().End);
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
    else {
        retType = new NothType(peek().Start, peek().Start);
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

    return createNode<FuncDeclStmt>(name, args, retType, body, accessCopy, firstTok.Start, peek().End);
}

Stmt *
Parser::parseUS() {
    const Token firstTok = advance();
    std::vector<NameObj> path;
    while (peek().Kind != TkSemi) {
        if (peek().Kind == TkId || peek().Kind == TkDot) {
            const Token tok = advance();
            if (tok.Kind != TkDot) {
                path.push_back(tok);
            }
        }
        else {
            _diag.Report(Error, "expected identifier of '.'")
                .SetCode(ErrExpectedToken)
                .AddSpan(peek().Start, peek().End);
            break;
        }
    }
    return createNode<UsingStmt>(path, firstTok.Start, peek().End);
}

Stmt *
Parser::parseRS() {
    const Token firstTok = advance();
    Expr *expr = nullptr;
    if (peek().Kind != TkSemi) {
        expr = parseExpr();
    }
    return createNode<RetStmt>(expr, firstTok.Start, peek().End);
}

Stmt *
Parser::parseIES() {
    const Token firstTok = advance();
    Expr *expr = parseExpr();
    if (!expect(TkLBrace)) {
        _diag.Report(Error, "expected '{'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    std::vector<Stmt *> thenBody;
    std::vector<Stmt *> elseBody;
    while (!expect(TkRBrace)) {
        thenBody.push_back(parseStmt());
    }
    if (expect(TkElse)) {
        if (expect(TkLBrace)) {
            while (!expect(TkRBrace)) {
                elseBody.push_back(parseStmt());
            }
        }
        else {
            elseBody.push_back(parseStmt());
        }
    }
    return createNode<IfElseStmt>(expr, thenBody, elseBody, firstTok.Start, peek(-1).End);
}

Stmt *
Parser::parseFLS() {
    const Token firstTok = advance();
    Stmt *indexator = nullptr;
    if (peek().Kind == TkVar || peek().Kind == TkId && isAssignmentOp(peek(1).Kind)) {
        indexator = parseStmt(false);
        if (!expect(TkComma)) {
            _diag.Report(Error, "expected ','")
                .SetCode(ErrExpectedToken)
                .AddSpan(peek().Start, peek().End);
        }
    }
    Expr *cond = parseExpr(PrecLowest, false);
    Stmt *iteration = nullptr;
    if (peek().Kind != TkLBrace) {
        if (!expect(TkComma)) {
            _diag.Report(Error, "expected ','")
                .SetCode(ErrExpectedToken)
                .AddSpan(peek().Start, peek().End);
        }
        iteration = parseStmt(false);
    }
    std::vector<Stmt *> block;
    if (!expect(TkLBrace)) {
        _diag.Report(Error, "expected '{'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    else {
        while (!expect(TkRBrace)) {
            block.push_back(parseStmt());
        }
    }
    return createNode<ForLoopStmt>(indexator, cond, iteration, block, Priv, firstTok.Start, peek(-1).End);
}

Stmt *
Parser::parseSDS() {
    AccessModifier accessCopy = access;
    const Token firstTok = advance();
    const Token nameTok = advance();
    if (nameTok.Kind != TkId) {
        _diag.Report(Error, "expected identifier")
            .SetCode(ErrExpectedId)
            .AddSpan(nameTok.Start, nameTok.End);
    }
    NameObj name = nameTok;
    if (!expect(TkLBrace)) {
        _diag.Report(Error, "expected '{'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    std::vector<StructDeclStmt::Field> fields;
    while (!expect(TkRBrace)) {
        fields.push_back(parseStructField());
    }
    return createNode<StructDeclStmt>(name, fields, accessCopy, firstTok.Start, peek(-1).End);
}

Stmt *
Parser::parseIS() {
    const Token firstTok = advance();
    Type *structOrTraitType = consumeType();
    Type *structType = nullptr;
    Type *traitType = nullptr;
    if (expect(TkFor)) {
        traitType = structOrTraitType;
        structType = consumeType();
    }
    else {
        structType = structOrTraitType;
    }

    if (!expect(TkLBrace)) {
        _diag.Report(Error, "expected '{'")
            .SetCode(ErrExpectedToken)
            .AddSpan(peek().Start, peek().End);
    }
    std::vector<ImplStmt::Method> methods;
    while (!expect(TkRBrace)) {
        methods.push_back(parseStructMethod());
    }

    return createNode<ImplStmt>(structType, traitType, methods, firstTok.Start, peek(-1).End);
}

Stmt *
Parser::parseDS() {
    const Token firstTok = advance();
    Expr *expr = parseExpr();
    return createNode<DelStmt>(expr);
}

StructDeclStmt::Field
Parser::parseStructField() {
    AccessModifier access = expect(TkPub) ? Pub : Priv;
    bool isStatic = expect(TkStatic);
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
    Expr *expr = nullptr;
    if (expect(TkEq)) {
        expr = parseExpr();
    }
    if (!expect(TkSemi)) {
        _diag.Report(Error, "expected ';'")
            .SetCode(ErrExpectedSemi)
            .AddSpan(peek().Start, peek().End)
            .AddHelp("add ';' to the end of the line");
    }
    return { name, type, access, isStatic, expr };
}

ImplStmt::Method
Parser::parseStructMethod() {
    AccessModifier access = expect(TkPub) ? Pub : Priv;
    bool isStatic = expect(TkStatic);
    const Token funcKeyword = advance();
    if (funcKeyword.Kind != TkFunc) {
        _diag.Report(Error, "expected 'func'")
            .SetCode(ErrExpectedToken)
            .AddSpan(funcKeyword.Start, funcKeyword.End);
    }
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
    else {
        retType = new NothType(peek().Start, peek().Start);
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

    return { name, args, retType, body, access, isStatic };
}

Expr *
Parser::parsePrefixExpr(bool allowStruct) {
    const Token tok = advance();
    Expr *base = nullptr;
    switch (tok.Kind) {
        #define LIT(t, v) createNode<LiteralExpr>(Value(Value::Const, ValueData(v), new t, tok.Start, tok.End))
        #define INT_LIT(bw, iu) LIT(IntegerType(bw, iu, tok.Start, tok.End), (int64_t)stoll(tok.Val))
        #define FLOAT_LIT(t) LIT(FloatingType(FloatingType::t, tok.Start, tok.End), (double)stold(tok.Val))
        #define SIZE_LIT(iu) LIT(SizeType(iu, tok.Start, tok.End), (int64_t)stoll(tok.Val))

        case TkBoolLit:
            base = LIT(IntegerType(1, false, tok.Start, tok.End), (int64_t)(tok.Val == "true"));
            break;
        case TkCharLit:
            base = LIT(CharType(tok.Start, tok.End), tok.Val);
            break;
        case TkI16Lit:
            base = INT_LIT(16, false);
            break;
        case TkI32Lit:
            base = INT_LIT(32, false);
            break;
        case TkI64Lit:
            base = INT_LIT(64, false);
            break;
        case TkI128Lit:
            base = INT_LIT(128, false);
            break;
        case TkISizeLit:
            base = SIZE_LIT(false);
            break;
        case TkU16Lit:
            base = INT_LIT(16, true);
            break;
        case TkU32Lit:
            base = INT_LIT(32, true);
            break;
        case TkU64Lit:
            base = INT_LIT(64, true);
            break;
        case TkU128Lit:
            base = INT_LIT(128, true);
            break;
        case TkUSizeLit:
            base = SIZE_LIT(true);
            break;
        case TkF32Lit:
            base = FLOAT_LIT(Float);
            break;
        case TkF64Lit:
            base = FLOAT_LIT(Double);
            break;
        case TkStrLit:
            base = LIT(StringType(tok.Start, tok.End), tok.Val);
            break;
        
        #undef SIZE_LIT
        #undef FLOAT_LIT
        #undef INT_LIT
        #undef LIT
        
        case TkLParen: {
            Expr *expr = parseExpr();
            expr->GetStartLoc() = tok.Start;
            expr->GetEndLoc() = peek(-1).End;
            // TODO: add handle of tuple initialization
            if (!expect(TkRParen)) {
                _diag.Report(Error, "expected ')'")
                    .SetCode(ErrExpectedToken)
                    .AddSpan(peek().Start, peek().End);
            }
            base = expr;
            break;
        }
        case TkLBracket: {
            std::vector<Expr *> exprs;
            while (!expect(TkRBracket)) {
                exprs.push_back(parseExpr());
                if (peek().Kind != TkRBracket) {
                    if (!expect(TkComma)) {
                        _diag.Report(Error, "expected ','")
                            .SetCode(ErrExpectedToken)
                            .AddSpan(peek().Start, peek().End);
                    }
                }
            }
            base = createNode<ArrayInstanceExpr>(exprs, tok.Start, peek(-1).End);
            break;
        }
        case TkMinus:
        case TkBang: {
            base = createNode<UnaryExpr>(tok, parsePrefixExpr(allowStruct), tok.Start, peek(-1).End);
            break;
        }
        case TkId: {
            std::vector<NameObj> path;
            path.push_back(tok);

            while (peek().Kind == TkDot && peek(1).Kind == TkId && peek(2).Kind != TkLParen) {
                advance();
                path.push_back(advance());
            }

            if (expect(TkLParen)) {
                std::vector<Expr *> args;
                parseArgsInto(args);
                return createNode<FuncCallExpr>(path[0], args, peek(-1).End);
            }
            else if (allowStruct && expect(TkLBrace)) {
                std::vector<std::pair<NameObj, Expr *>> fields;
                while (!expect(TkRBrace)) {
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
                    Expr *expr = parseExpr();
                    if (peek().Kind != TkRBrace) {
                        if (!expect(TkComma)) {
                            _diag.Report(Error, "expected ','")
                                .SetCode(ErrExpectedToken)
                                .AddSpan(peek().Start, peek().End);
                        }
                    }
                    fields.push_back({ name, expr });
                }
                return createNode<StructInstanceExpr>(path, fields, peek(-1).End);
            }

            Expr *expr = createNode<VarExpr>(path[0]);
            for (int i = 1; i < path.size(); ++i) {
                expr = createNode<FieldExpr>(expr, path[i]);
            }
            
            if (peek().Kind == TkDot || peek().Kind == TkLBracket) {
                expr = parseChain(expr);
            }
            return expr;
        }
        case TkAnd: {
            Expr *expr = parsePrefixExpr(allowStruct);
            base = createNode<RefExpr>(expr);
            base->GetStartLoc() = tok.Start;
            break;
        }
        case TkStar: {
            Expr *expr = parsePrefixExpr(allowStruct);
            base = createNode<DerefExpr>(expr);
            base->GetStartLoc() = tok.Start;
            break;
        }
        case TkNew: {
            Type *type = consumeType();
            Expr *expr = nullptr;
            if (expect(TkLParen)) {
                expr = parseExpr();
                if (!expect(TkRParen)) {
                    _diag.Report(Error, "expected ')'")
                        .SetCode(ErrExpectedToken)
                        .AddSpan(peek().Start, peek().End);
                }
            }
            base = createNode<NewExpr>(type, expr, tok.Start, expr ? expr->GetEndLoc() : type->GetEndLoc());
            break;
        }
        case TkChar: {
            base = createNode<TypeExpr>(new CharType(tok.Start, tok.End));
            break;
        }
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
            if (tok.Kind == TkBool) {
                base = createNode<TypeExpr>(new IntegerType(1, false, tok.Start, tok.End));
            }
            bool isUnsigned = tok.Kind >= TkU8;
            unsigned bitWidth = 1 << ((isUnsigned ? tok.Kind - TkU8 : tok.Kind - TkI8) + 3);
            base = createNode<TypeExpr>(new IntegerType(bitWidth, isUnsigned, tok.Start, tok.End));
            break;
        }
        case TkISize:
        case TkUSize:
            base = createNode<TypeExpr>(new SizeType(tok.Kind == TkUSize, tok.Start, tok.End));
            break;
        case TkF32:
        case TkF64:
            base = createNode<TypeExpr>(new FloatingType((FloatingType::FloatingKind)(tok.Kind - TkF32), tok.Start, tok.End));
            break;
        case TkString:
            base = createNode<TypeExpr>(new StringType(tok.Start, tok.End));
            break;
        case TkNoth:
            base = createNode<TypeExpr>(new NothType(tok.Start, tok.End));
            break;
        case TkNil:
            return createNode<NilExpr>(tok.Start, tok.End);
    }
    if (!base) {
        _diag.Report(Error, "expected expression")
            .SetCode(ErrExpectedExpr)
            .AddSpan(tok.Start, tok.End);
        return nullptr;
    }
    while (peek().Kind == TkDot || peek().Kind == TkLParen || peek().Kind == TkLBracket) {
        base = parseChain(base); 
    }
    return base;
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
        lhs = createNode<BinaryExpr>(lhs, op, rhs, lhs->GetStartLoc(), peek(-1).End);
    }

    return lhs;
}

Expr *
Parser::parseChain(Expr *base) {
    while (true) {
        if (expect(TkDot)) {
            const Token id = advance();
            if (id.Kind != TkId) {
                _diag.Report(Error, "expected identifier")
                    .SetCode(ErrExpectedId)
                    .AddSpan(id.Start, id.End);
            }
            NameObj name = id;
            if (expect(TkLParen)) {
                std::vector<Expr *> args;
                parseArgsInto(args);
                base = createNode<MethodCallExpr>(base, name, args, peek(-1).End);
            }
            else {
                base = createNode<FieldExpr>(base, name);
            }
        }
        else if (expect(TkLBracket)) {
            Expr *index = parseExpr();
            if (!expect(TkRBracket)) {
                _diag.Report(Error, "expected ']'")
                    .SetCode(ErrExpectedToken)
                    .AddSpan(peek().Start, peek().End);
            }
            base = createNode<ArrayAccessExpr>(base, index, peek(-1).End);
        }
        else {
            break;
        }
    }
    return base;
}

Expr *
Parser::createCompoundAssignmentOp(Token op, Expr *base, Expr *expr) {
    switch (op.Kind) {
        #define OP(k, v) createNode<BinaryExpr>(base, Token(k, v, op.Start, op.End), expr, base->GetStartLoc(), expr->GetEndLoc())
        case TkPlusEq:
            return OP(TkPlus, "+");
        case TkMinusEq:
            return OP(TkMinus, "-");
        case TkStarEq:
            return OP(TkStar, "*");
        case TkSlashEq:
            return OP(TkSlash, "/");
        case TkPercentEq:
            return OP(TkPercent, "%");
        default:
            return nullptr;
        #undef OP
    }
}

void
Parser::parseArgsInto(std::vector<Expr *> &args) {
    while (!expect(TkRParen)) {
        args.push_back(parseExpr());
        if (peek().Kind != TkRParen) {
            if (!expect(TkComma)) {
                _diag.Report(Error, "expected ','")
                    .SetCode(ErrExpectedToken)
                    .AddSpan(peek().Start, peek().End);
            }
        }
    }
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
            std::vector<NameObj> path;
            path.push_back(c);
            
            while (peek().Kind == TkDot && peek(1).Kind == TkId) {
                advance();
                path.push_back(advance());
            }
            return new UnknownNamedType(path, c.Start, path.back().End);
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
            if (size) {
                return new ArrayType(base, size, c.Start, peek(-1).End);
            }
            return new SliceType(base, c.Start, peek(-1).End);
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
        }
    }
    _diag.Report(Error, "expected type")
        .SetCode(ErrExpectedType)
        .AddSpan(c.Start, c.End);
    return nullptr;
}

const Token
Parser::peek(uint rpos) {
    if (_pos + rpos < _tokens.size()) {
        return _tokens[_pos + rpos];
    }
    if (_tokens.size()) {
        auto srcMgr = _diag.GetSourceMgr();
        unsigned bufferId = srcMgr->FindBufferContainingLoc(_tokens[0].Start);
        const char *end = srcMgr->getBufferInfo(bufferId).Buffer->getBufferEnd();
        return Token(TkEof, "", llvm::SMLoc::getFromPointer(end), llvm::SMLoc::getFromPointer(end));
    }
    return Token(TkEof, "", llvm::SMLoc(), llvm::SMLoc());
}

const Token
Parser::advance() {
    return peek(-_pos + _pos++);
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
