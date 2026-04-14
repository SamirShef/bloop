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

        SemanticResult() : Val(Value::GetIncorrectValue()), HirNode(nullptr) {}
        SemanticResult(Value v, HIRNode *h) : Val(v), HirNode(h) {}
    };

    enum CastCost {
        Exact = 0,
        SafeImplicit = 1,
        Incompatible = 1000
    };

    struct Scope {
        std::unordered_map<std::string, Variable> Vars;
    };
    std::stack<Scope> _vars;
    unsigned _currentFuncVarCount = 0;

    struct LoopFrame {
        HIRBasicBlock *Break;
        HIRBasicBlock *Continue;
    };
    std::stack<LoopFrame> _loops;

    std::unordered_map<std::string, int> _staticFields; // <mangled name, index in globals>

public:
    explicit Semantic(DiagnosticEngine &d, Module *&m, HIRContext &c, const std::unordered_map<std::string, FileNode> &g)
        : _diag(d), _mod(m), _builder(c), _graph(g) {
        _vars.push({});
    }

    void
    Analyze(std::vector<Stmt *> &ast) {
        for (auto &s : ast) {
            if (!s) {
                continue;
            }
            if (s->GetKind() == NkStructDeclStmt) {
                analyzeSDS(llvm::cast<StructDeclStmt>(s));
            }
        }

        for (auto &s : ast) {
            if (!s) {
                continue;
            }
            if (s->GetKind() == NkFuncDeclStmt) {
                registerFunc(llvm::cast<FuncDeclStmt>(s));
            }
            else if (s->GetKind() == NkImplStmt) {
                registerImplMethods(llvm::cast<ImplStmt>(s));
            }
        }

        for (auto &s : ast) {
            if (!s || s->GetKind() == NkStructDeclStmt) {
                continue;
            }
            analyzeStmt(s);
        }
    }

    HIRContext &
    GetContext() const {
        return _builder.GetContext();
    }

private:
    void
    analyzeStmt(Stmt *stmt);

    void
    analyzeVDS(VarDeclStmt *vds);

    void
    analyzeVAS(VarAsgnStmt *vas);

    void
    analyzeFAS(FieldAsgnStmt *fas);
    
    void
    analyzeFDS(FuncDeclStmt *fds);
    
    void
    registerFunc(FuncDeclStmt *fds);
    
    void
    resolveFuncSignature(Function *func);

    void
    analyzeFuncBody(FuncDeclStmt *fds);

    void
    analyzeFCS(FuncCallStmt *fcs);

    void
    analyzeMCS(MethodCallStmt *mcs);

    void
    analyzeUS(UsingStmt *us);

    void
    analyzeRS(RetStmt *rs);

    void
    analyzeIES(IfElseStmt *ies);

    void
    analyzeFLS(ForLoopStmt *fls);

    void
    analyzeBS(BreakStmt *bs);

    void
    analyzeCS(ContinueStmt *cs);

    void
    analyzeSDS(StructDeclStmt *sds);

    void
    registerImplMethods(ImplStmt *is);

    void
    registerMethod(Struct *s, ImplStmt::Method *method);

    void
    resolveMethodSignature(Struct *s, Method *method, ImplStmt::Method *methodObj);

    void
    analyzeMethodBody(Struct *s, Method *method, ImplStmt::Method *methodObj);

    void
    analyzeIS(ImplStmt *is);

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

    SemanticResult
    analyzeFCE(FuncCallExpr *fce);

    SemanticResult
    analyzeFE(FieldExpr *fe);

    SemanticResult
    analyzeMCE(MethodCallExpr *mce);

    SemanticResult
    analyzeSIE(StructInstanceExpr *sie);

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

    Struct *
    findStructByPath(const std::vector<NameObj> &path) {
        if (path.empty()) {
            return nullptr;
        }
        if (path.size() == 1) {
            return findStruct(path[0].Name);
        }

        Module *currentMod = nullptr;
        if (_mod->Imports.count(path[0].Name)) {
            currentMod = _mod->Imports[path[0].Name];
        }
        else if (_mod->Submods.count(path[0].Name)) {
            currentMod = _mod->Submods[path[0].Name];
        }
        else {
            return nullptr;
        }

        for (int i = 1; i < path.size() - 1; ++i) {
            if (currentMod->Submods.count(path[i].Name)) {
                currentMod = currentMod->Submods[path[i].Name];
            }
            else {
                _diag.Report(Error, "symbol '" + path[i].Name + "' is undeclared in module '" + currentMod->ToString() + "'")
                    .SetCode(ErrUndeclaredSymbol)
                    .AddSpan(path[i].Start, path[i].End, "undeclared");
                return nullptr;
            }
        }

        if (currentMod->Structs.count(path.back().Name)) {
            auto *s = &currentMod->Structs.at(path.back().Name);
            if (s->Access == Pub) {
                return s;
            }
            else {
                _diag.Report(Error, "symbol '" + s->Name.Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(s->Name.Start, s->Name.End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make struct '" + s->Name.Name + "' accessible");
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

    CastCost
    checkCastCost(Type *src, Type *dst);

    Function *
    resolveBestOverload(FuncOverload *candidates, const std::vector<Type *> &argTypes, llvm::SMLoc start, llvm::SMLoc end);

    HIRBinaryKind
    tokenKindToHIRBk(TokenKind kind);

    HIRUnaryKind
    tokenKindToHIRUk(TokenKind kind);
};

}