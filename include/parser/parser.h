#pragma once
#include <utils/symbols/function.h>
#include <parser/precedence.h>
#include <utils/types/type.h>
#include <diag/engine.h>
#include <lexer/token.h>
#include <ast/stmt.h>
#include <ast/expr.h>
#include <vector>

namespace bloop {

class Parser {
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
    Stmt *
    parseStmt(bool consumeSemi = true);

    Stmt *
    parseVDS();

    Stmt *
    parseFDS();

    Stmt *
    parseUS();

    Stmt *
    parseRS();
    
    Expr *
    parsePrefixExpr(bool allowStruct = true);

    Expr *
    parseExpr(int minPrec = PrecLowest, bool allowStruct = true);
    
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