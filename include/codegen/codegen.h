#pragma once
#include <hir/hir.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace bloop {

class CodeGen {
    std::vector<HIRNode *> &_nodes;
    llvm::LLVMContext _context;
    llvm::Module *_module;
    llvm::IRBuilder<> _builder;
    std::vector<llvm::GlobalVariable *> _globals;

    struct Function {
        llvm::Function *Func;
        std::vector<llvm::AllocaInst *> Locals;
    };
    std::unordered_map<std::string, Function> _funcsMap;
    std::vector<llvm::Function *> _funcs;
    llvm::Function *_userMainFunc = nullptr;

public:
    explicit CodeGen(std::string name, std::vector<HIRNode *> &n) : _nodes(n), _context(), _builder(_context), _module(new llvm::Module(name, _context)) {}

    llvm::Module *
    Generate() {
        for (auto &n : _nodes) {
            generateNode(n);
        }
        if (_userMainFunc) {
            generateImplicitMain();
        }
        return _module;
    }

private:
    void
    generateNode(HIRNode *node);

    void
    generateVDS(HIRVarDeclStmt *vds);

    void
    generateFDS(HIRFuncDeclStmt *fds);
    
    void
    generateRS(HIRRetStmt *rs);

    void
    generateImplicitMain();

    llvm::Value *
    generateExpr(HIRNode *expr);
    
    llvm::Value *
    generateBE(HIRBinaryExpr *be);
    
    llvm::Value *
    generateLE(HIRLiteralExpr *le);
    
    llvm::Value *
    generateUE(HIRUnaryExpr *ue);
    
    llvm::Value *
    generateVE(HIRVarExpr *ve);
    
    llvm::Value *
    generateCast(HIRCastNode *cast);

    llvm::Value *
    generateFCE(HIRFuncCallExpr *fce);
    
    llvm::Type *
    getType(Type *type);
};

}