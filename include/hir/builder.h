#pragma once
#include <hir/node.h>
#include <hir/hir.h>
#include <llvm/Support/Allocator.h>
#include <vector>

namespace bloop {

class HIRContext {
    llvm::BumpPtrAllocator _allocator;
    std::vector<HIRFuncDeclStmt *> _functions;
    std::vector<HIRVarDeclStmt *> _globals;

public:
    HIRContext() = default;
    
    HIRContext(const HIRContext &) = delete;

    HIRContext &
    operator=(const HIRContext &) = delete;

    template<typename T, typename... Args>
    T *
    CreateNode(Args &&... args) {
        void* ptr = _allocator.Allocate<T>();
        return new (ptr) T(std::forward<Args>(args)...);
    }


    std::vector<HIRFuncDeclStmt *> &
    GetFunctions() {
        return _functions;
    }

    void
    AddFunction(HIRFuncDeclStmt *func) {
        _functions.push_back(func);
    }
    
    std::vector<HIRVarDeclStmt *> &
    GetGlobals() {
        return _globals;
    }
    
    void
    AddGlobal(HIRVarDeclStmt *var) {
        _globals.push_back(var);
    }
};
    
class HIRBuilder {
    HIRContext &_ctx;
    HIRBasicBlock* _insertBlock = nullptr;

public:
    explicit HIRBuilder(HIRContext &c) : _ctx(c) {}

    HIRContext &
    GetContext() const {
        return _ctx;
    }

    HIRBasicBlock *
    CreateBlock(HIRFuncDeclStmt *parent, std::string name = "") {
        auto *bb = _ctx.CreateNode<HIRBasicBlock>(name, parent);
        parent->GetBody().push_back(bb);
        return bb;
    }

    HIRFuncDeclStmt *
    GetParent() const {
        return _insertBlock->GetParent();
    }

    void
    SetInsertPoint(HIRBasicBlock* bb) {
        _insertBlock = bb;
    }

    void
    AddToBlock(HIRNode *node) {
        assert(_insertBlock && "No insertion block set!");
        if (node->IsTerminator()) {
            _insertBlock->SetTerminator(node);
        }
        else {
            _insertBlock->AddInstruction(node);
        }
    }

    HIRVarDeclStmt *
    CreateVar(std::string name, Type *type, HIRNode *expr, StorageKind kind, bool isConst = false) {
        auto *node = _ctx.CreateNode<HIRVarDeclStmt>(name, type, expr, isConst, kind);
        if (_insertBlock) {
            _insertBlock->AddInstruction(node);
        }
        else {
            _ctx.AddGlobal(node);
        }
        return node;
    }

    HIRFuncDeclStmt *
    CreateFunc(std::string name, Type *retType, std::vector<HIRFuncArgument> args, bool isMain = false, bool isDeclaration = false) {
        auto *node = _ctx.CreateNode<HIRFuncDeclStmt>(name, retType, args, isMain, isDeclaration);
        _ctx.AddFunction(node);
        return node;
    }

    HIRCastNode *
    CreateCast(CastKind kind, HIRNode *expr, Type *from, Type *to) {
        return _ctx.CreateNode<HIRCastNode>(kind, expr, from, to);
    }

    HIRBranch *
    CreateBr(HIRBasicBlock *thenBlock) {
        auto *node = _ctx.CreateNode<HIRBranch>(thenBlock);
        AddToBlock(node);
        return node;
    }

    HIRBranch *
    CreateBr(HIRNode *cond, HIRBasicBlock *thenBlock, HIRBasicBlock *elseBlock) {
        auto *node = _ctx.CreateNode<HIRBranch>(cond, thenBlock, elseBlock);
        AddToBlock(node);
        return node;
    }

    HIRLiteralExpr *
    CreateLiteral(Value val) {
        return _ctx.CreateNode<HIRLiteralExpr>(val);
    }

    HIRBinaryExpr *
    CreateBinary(Type *commonType, HIRNode *lhs, HIRNode *rhs, HIRBinaryKind op) {
        return _ctx.CreateNode<HIRBinaryExpr>(commonType, lhs, rhs, op);
    }

    HIRUnaryExpr *
    CreateUnary(HIRNode *rhs, HIRUnaryKind op) {
        return _ctx.CreateNode<HIRUnaryExpr>(rhs, op);
    }

    HIRVarExpr *
    CreateLoadVar(StorageKind kind, uint index, Module *parent = nullptr) {
        return _ctx.CreateNode<HIRVarExpr>(kind, index, parent);
    }

    HIRFuncCallExpr *
    CreateCall(std::string name, std::vector<HIRNode *> &args, Module *parent = nullptr) {
        return _ctx.CreateNode<HIRFuncCallExpr>(name, args, parent);
    }

    HIRRetStmt *
    CreateRet(Type *type, HIRNode *expr) {
        assert(_insertBlock && "Attempting to create Return outside of function!");
        auto *node = _ctx.CreateNode<HIRRetStmt>(type, expr);
        AddToBlock(node);
        return node;
    }

    HIRNode *
    GetIncorrectValue() {
        return CreateLiteral(Value::GetIncorrectValue());
    }
};

}