#pragma once
#include <hir/builder.h>
#include <hir/node.h>
#include <utils/modules/module.h>
#include <ast/ast.h>
#include <diag/engine.h>
#include <utils/symbols/variable.h>
#include <stack>
#include <string>
#include <unordered_map>

namespace bloop {

class Semantic {
    DiagnosticEngine &_diag;
    std::stack<Type *> _funcsRetTypes;
    Module *&_mod;
    HIRBuilder _builder;
    
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
    explicit Semantic(DiagnosticEngine &d, Module *&m) : _diag(d), _mod(m), _builder(HIRContext()) {
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

    #define FIND(t, n, map) t *n(std::string name) { \
        auto it = _mod->map.find(name); \
        return it == _mod->map.end() ? nullptr : &it->second; \
    }

    FIND(Variable, findGlobVar, Vars)
    FIND(FuncOverload, findFuncCandidates, FuncOverloads)
    FIND(Struct, findStruct, Structs)
    FIND(Trait, findTrait, Traits)

    #undef FIND

    void
    createVar(std::string name, Variable var);

    Type *
    resolveType(Type **t);

    Type *
    getCommonType(Type *lhs, Type *rhs);

    Type *
    getCommonTypeForOp(Type *lhs, Type *rhs, const Token op, llvm::SMLoc s = llvm::SMLoc(), llvm::SMLoc e = llvm::SMLoc());

    Value
    implicitlyCast(Value val, Type **expectedType);

    HIRBinaryKind
    tokenKindToHIRBk(TokenKind kind);

    HIRUnaryKind
    tokenKindToHIRUk(TokenKind kind);
};

}