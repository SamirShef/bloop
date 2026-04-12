#pragma once
#include <hir/hir.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace bloop {

class CodeGen {
    std::vector<HIRStructDeclStmt *> &_hirStructs;
    std::vector<HIRVarDeclStmt *> &_hirGlobals;
    std::vector<HIRFuncDeclStmt *> &_hirFunctions;
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
    explicit CodeGen(std::string name, std::vector<HIRStructDeclStmt *> &s, std::vector<HIRVarDeclStmt *> &g, std::vector<HIRFuncDeclStmt *> &f)
        : _hirStructs(s), _hirGlobals(g), _hirFunctions(f), _context(), _builder(_context), _module(new llvm::Module(name, _context)) {}

    llvm::Module *
    Generate() {
        for (auto &s : _hirStructs) {
            generateSDS(s);
        }
        
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
    generateLValue(HIRNode *node);

    llvm::Value *
    generateVDS(HIRVarDeclStmt *vds);

    llvm::Value *
    generateVarStore(HIRVarStore *varStore);

    llvm::Value *
    generateFieldStore(HIRFieldStore *fieldStore);
    
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

    llvm::Value *
    generateSDS(HIRStructDeclStmt *sds);

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

    llvm::Value *
    generateSIE(HIRStructInstanceExpr *sie);

    llvm::Value *
    generateFE(HIRFieldExpr *fe);
    
    llvm::Type *
    getType(Type *type);
};

}