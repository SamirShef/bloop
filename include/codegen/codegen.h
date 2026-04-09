#pragma once
#include <hir/hir.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace bloop {

class CodeGen {
    std::vector<HIRFuncDeclStmt *> &_hirFunctions;
    std::vector<HIRVarDeclStmt *> &_hirGlobals;
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

    std::unordered_map<HIRBasicBlock *, llvm::BasicBlock *> _blockMap;

public:
    explicit CodeGen(std::string name, std::vector<HIRVarDeclStmt *> &g, std::vector<HIRFuncDeclStmt *> &f)
        : _hirGlobals(g), _hirFunctions(f), _context(), _builder(_context), _module(new llvm::Module(name, _context)) {}

    llvm::Module *
    Generate() {
        for (auto &f : _hirFunctions) {
            declareFDS(f);
        }

        for (auto &g : _hirGlobals) {
            generateVDS(g);
        }
        
        for (auto &f : _hirFunctions) {
            generateNode(f);
        }

        if (_userMainFunc) {
            generateImplicitMain();
        }

        return _module;
    }

private:
    llvm::Value *
    generateNode(HIRNode *node);

    llvm::Value *
    generateVDS(HIRVarDeclStmt *vds);

    void
    declareFDS(HIRFuncDeclStmt *fds);

    llvm::Value *
    generateFDS(HIRFuncDeclStmt *fds);
    
    llvm::Value *
    generateRS(HIRRetStmt *rs);

    llvm::Value *
    generateBB(HIRBasicBlock *bb);
    
    llvm::Value *
    generateBR(HIRBranch *br);

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