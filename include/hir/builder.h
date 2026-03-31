#pragma once
#include <hir/node.h>
#include <hir/hir.h>
#include <vector>

namespace bloop {

class HIRContext {
    std::vector<HIRNode *> _nodes;
    HIRFuncDeclStmt *_curFunc = nullptr;

public:
    std::vector<HIRNode *> &
    GetNodes() {
        return _nodes;
    }

    template<typename T = HIRNode *>
    T
    AddNode(T node) {
        if (_curFunc) {
            _curFunc->GetBody().push_back(node);
        }
        else {
            _nodes.push_back(node);
        }
        return node;
    }

    HIRFuncDeclStmt *&
    GetCurFunc() {
        return _curFunc;
    }
};
    
class HIRBuilder {
    HIRContext _context;

public:
    explicit HIRBuilder(HIRContext c) : _context(c) {}

    HIRVarDeclStmt *
    CreateVar(std::string name, Type *type, HIRNode *expr, bool isConst = false) {
        return _context.AddNode(new HIRVarDeclStmt(name ,type, expr, isConst));
    }

    HIRFuncDeclStmt *
    CreateFunc(std::string name, Type *retType, std::vector<HIRFuncArgument> args) {
        auto *func = _context.AddNode(new HIRFuncDeclStmt(name, retType, args));
        _context.GetCurFunc() = func;
        return func;
    }

    HIRCastNode *
    CreateCast(CastKind kind, HIRNode *expr) {
        return _context.AddNode(new HIRCastNode(kind, expr));
    }

    HIRLiteralExpr *
    CreateLiteral(Value val) {
        return new HIRLiteralExpr(val);
    }

    HIRBinaryExpr *
    CreateBinary(Type *commonType, HIRNode *lhs, HIRNode *rhs, HIRBinaryKind op) {
        return new HIRBinaryExpr(commonType, lhs, rhs, op);
    }

    HIRUnaryExpr *
    CreateUnary(HIRNode *rhs, HIRUnaryKind op) {
        return new HIRUnaryExpr(rhs, op);
    }

    HIRVarExpr *
    CreateLoadVar(StorageKind kind, uint index) {
        return new HIRVarExpr(kind, index);
    }

    HIRNode *
    GetIncorrectValue() {
        return CreateLiteral(Value::GetIncorrectValue());
    }
};

}