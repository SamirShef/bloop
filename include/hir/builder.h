#pragma once
#include <hir/node.h>
#include <hir/hir.h>
#include <vector>

namespace bloop {

class HIRContext {
    std::vector<HIRNode *> _nodes;

public:
    std::vector<HIRNode *> &
    GetNodes() {
        return _nodes;
    }
};
    
class HIRBuilder {
    HIRContext _context;

public:
    explicit HIRBuilder(HIRContext c) : _context(c) {}

    HIRVarDeclStmt *
    CreateGlobalVar(std::string name, Type *type, HIRNode *expr, bool isConst = false);

    HIRVarDeclStmt *
    CreateLocalVar(std::string name, Type *type, HIRNode *expr, bool isConst = false);

    HIRFuncDeclStmt *
    CreateFunc(std::string name, Type *retType, std::vector<HIRFuncArgument> args, std::vector<HIRNode *> body);

    HIRCastNode *
    CreateCast(CastKind kind, HIRNode *expr);

    HIRLiteralExpr *
    CreateLiteral(Value val);

    HIRBinaryExpr *
    CreateBinary(Type *commonType, HIRNode *lhs, HIRNode *rhs, TokenKind op);

    HIRUnaryExpr *
    CreateUnary(HIRNode *rhs, TokenKind op);

    HIRVarExpr *
    CreateLoadVar(StorageKind kind, uint index);
};

}