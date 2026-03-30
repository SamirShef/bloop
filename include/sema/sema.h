#pragma once
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
    std::stack<std::unordered_map<std::string, Variable>> _vars;
    std::stack<Type *> _funcsRetTypes;
    Module *&_mod;

public:
    Semantic(DiagnosticEngine &d, Module *&m) : _diag(d), _mod(m) {
        _vars.push({});
    }

    void
    Analyze(std::vector<Stmt *> &ast) {
        for (auto &s : ast) {
            analyzeStmt(s);
        }
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

    Value
    analyzeExpr(Expr *expr);

    Value
    analyzeBE(BinaryExpr *be);

    Value
    analyzeLE(LiteralExpr *le);

    Value
    analyzeUE(UnaryExpr *ue);

    Value
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

    Type *
    resolveType(Type **t);

    Type *
    getCommonType(Type *lhs, Type *rhs);

    Type *
    getCommonTypeForOp(Type *lhs, Type *rhs, const Token op, llvm::SMLoc s = llvm::SMLoc(), llvm::SMLoc e = llvm::SMLoc());

    Value
    implicitlyCast(Value val, Type **expectedType);
};

}