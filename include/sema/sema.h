#pragma once
#include <hir/builder.h>
#include <hir/node.h>
#include <ast/ast.h>
#include <diag/engine.h>
#include <utils/symbols/variable.h>
#include <utils/modules/module.h>
#include <utils/modules/fileNode.h>
#include <stack>
#include <string>
#include <unordered_map>

namespace bloop {

class Semantic {
    DiagnosticEngine &_diag;
    std::stack<Type *> _funcsRetTypes;
    Module *&_mod;
    HIRBuilder _builder;
    const std::unordered_map<std::string, FileNode> &_graph;
    
    struct SemanticResult {
        Value Val;
        HIRNode *HirNode;
    };

    struct Scope {
        std::unordered_map<std::string, int> VarsMap;
        std::vector<Variable> Vars;
    };
    std::stack<Scope> _vars;

public:
    explicit Semantic(DiagnosticEngine &d, Module *&m, const std::unordered_map<std::string, FileNode> &g)
        : _diag(d), _mod(m), _builder(HIRContext()), _graph(g) {
        _vars.push({});
    }

    void
    Analyze(std::vector<Stmt *> &ast) {
        for (auto &s : ast) {
            analyzeStmt(s);
        }
    }

    HIRContext
    GetContext() const {
        return _builder.GetContext();
    }

private:
    void
    analyzeStmt(Stmt *stmt);

    void
    analyzeVDS(VarDeclStmt *vds);
    
    void
    analyzeFDS(FuncDeclStmt *fds);

    void
    analyzeUS(UsingStmt *us);

    void
    analyzeRS(RetStmt *rs);

    SemanticResult
    analyzeExpr(Expr *expr);

    SemanticResult
    analyzeBE(BinaryExpr *be);

    SemanticResult
    analyzeLE(LiteralExpr *le);

    SemanticResult
    analyzeUE(UnaryExpr *ue);

    SemanticResult
    analyzeVE(VarExpr *ve);

    Variable *
    findGlobVar(std::string name) {
        auto it = _mod->Vars.find(name);
        if (it != _mod->Vars.end()) {
            return &it->second;
        }
        for (auto &[modName, modPtr] : _mod->Imports) {
            auto impIt = modPtr->Vars.find(name);
            if (impIt != modPtr->Vars.end() && impIt->second.Access == Pub) {
                return &impIt->second;
            }
        }
        return nullptr;
    }

    FuncOverload *
    findFuncCandidates(std::string name) {
        auto it = _mod->FuncOverloads.find(name);
        if (it != _mod->FuncOverloads.end()) {
            return &it->second;
        }
        for (auto &[modName, modPtr] : _mod->Imports) {
            auto impIt = modPtr->FuncOverloads.find(name);
            if (impIt != modPtr->FuncOverloads.end()) {
                return &impIt->second;
            }
        }
        return nullptr;
    }

    Struct *
    findStruct(std::string name) {
        auto it = _mod->Structs.find(name);
        if (it != _mod->Structs.end()) {
            return &it->second;
        }
        for (auto &[modName, modPtr] : _mod->Imports) {
            auto impIt = modPtr->Structs.find(name);
            if (impIt != modPtr->Structs.end() && impIt->second.Access == Pub) {
                return &impIt->second;
            }
        }
        return nullptr;
    }

    Trait *
    findTrait(std::string name) {
        auto it = _mod->Traits.find(name);
        if (it != _mod->Traits.end()) {
            return &it->second;
        }
        for (auto &[modName, modPtr] : _mod->Imports) {
            auto impIt = modPtr->Traits.find(name);
            if (impIt != modPtr->Traits.end() && impIt->second.Access == Pub) {
                return &impIt->second;
            }
        }
        return nullptr;
    }

    void
    createVar(std::string name, Variable var);

    Type *
    resolveType(Type **t);

    Type *
    getCommonType(Type *lhs, Type *rhs);

    Type *
    getCommonTypeForOp(Type *lhs, Type *rhs, const Token op, llvm::SMLoc s = llvm::SMLoc(), llvm::SMLoc e = llvm::SMLoc());

    SemanticResult
    implicitlyCast(SemanticResult res, Type **expectedType);

    HIRBinaryKind
    tokenKindToHIRBk(TokenKind kind);

    HIRUnaryKind
    tokenKindToHIRUk(TokenKind kind);
};

}