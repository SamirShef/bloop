#pragma once
#include <utils/symbols/storageKind.h>
#include <utils/name.h>
#include <ast/expr.h>
#include <utils/symbols/access.h>
#include <utils/types/type.h>
#include <vector>

namespace bloop {

class Module;

enum AnalysisStatus {
    NotAnalyzed,
    ResolvingSig,
    SignatureReady,
    BodyAnalyzed
};

struct Argument {
    NameObj     Name;
    class Type *Type;
    Expr       *DefaultVal;

    explicit Argument(NameObj n, class Type *t, Expr *dv = nullptr) : Name(n), Type(t), DefaultVal(dv) {}

    bool
    operator==(const Argument &rhs) const {
        return Name.Name == rhs.Name.Name && *Type == *rhs.Type;
    }
};

struct Function {
    NameObj                 Name;
    Type                   *RetType;
    std::vector<Argument>   Args;
    AccessModifier          Access;
    uint                    RefsCount   = 0;
    StorageKind             Storage;
    Module                 *Parent      = nullptr;

    class FuncDeclStmt     *ASTNode     = nullptr; 
    AnalysisStatus          Status      = NotAnalyzed;
    class HIRFuncDeclStmt  *HirNode     = nullptr;

    explicit Function(NameObj n, Type *rt, std::vector<Argument> &ar, AccessModifier ac, StorageKind s = Static, Module *m = nullptr)
        : Name(n), RetType(rt), Args(ar), Access(ac), Storage(s), Parent(m) {}
    
    std::string
    GetMangledName() const;

    bool
    operator==(const Function &rhs) const {
        return Name.Name == rhs.Name.Name && *RetType == *rhs.RetType && Args == rhs.Args && Access == rhs.Access && Storage == rhs.Storage;
    }
};

struct FuncOverload {
    std::vector<Function> Candidates;

    bool
    operator==(const FuncOverload &rhs) const {
        return Candidates == rhs.Candidates;
    }
};

}