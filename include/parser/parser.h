#pragma once
#include <llvm/Support/Allocator.h>
#include <utils/symbols/function.h>
#include <parser/precedence.h>
#include <utils/types/type.h>
#include <diag/engine.h>
#include <lexer/token.h>
#include <ast/stmts/sds.h>
#include <ast/stmts/is.h>
#include <ast/exprs/ve.h>
#include <ast/exprs/fe.h>
#include <ast/exprs/de.h>
#include <ast/exprs/aae.h>
#include <vector>

namespace bloop {

class Parser {
    llvm::BumpPtrAllocator _allocator;
    DiagnosticEngine &_diag;
    std::vector<Token> &_tokens;
    uint _pos = 0;

public:
    explicit Parser(DiagnosticEngine &d, std::vector<Token> &t) : _diag(d), _tokens(t) {}

    void
    ParseInto(std::vector<Stmt *> &stmts) {
        while (_pos < _tokens.size()) {
            stmts.push_back(parseStmt());
        }
    }

private:
    template<typename T, typename... Args>
    T *
    createNode(Args &&... args) {
        void *ptr = _allocator.Allocate<T>();
        return new (ptr) T(std::forward<Args>(args)...);
    }

    Stmt *
    parseStmt(bool consumeSemi = true);

    Stmt *
    getStmtFromExpr(Expr *expr, bool consumeSemi);

    Stmt *
    parseVDS();

    Stmt *
    parseVAS(VarExpr *base);

    Stmt *
    parseFAS(FieldExpr *base);

    Stmt *
    parseDAS(DerefExpr *base);

    Stmt *
    parseAAS(ArrayAccessExpr *base);

    Stmt *
    parseFDS();

    Stmt *
    parseUS();

    Stmt *
    parseRS();

    Stmt *
    parseIES();

    Stmt *
    parseFLS();

    Stmt *
    parseSDS();

    Stmt *
    parseIS();

    Stmt *
    parseDS();
    
    StructDeclStmt::Field
    parseStructField();

    ImplStmt::Method
    parseStructMethod();
    
    Expr *
    parsePrefixExpr(bool allowStruct = true);

    Expr *
    parseExpr(int minPrec = PrecLowest, bool allowStruct = true);

    Expr *
    parseChain(Expr *base);

    Expr *
    createCompoundAssignmentOp(Token op, Expr *base, Expr *expr);

    void
    parseArgsInto(std::vector<Expr *> &args);
    
    Argument
    parseArgument();

    Type *
    consumeType();

    const Token
    peek(uint rpos = 0);

    const Token
    advance();

    bool
    expect(TokenKind kind);
};

}