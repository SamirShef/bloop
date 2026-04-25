#include <utils/types/types.h>
#include <utils/splitString.h>
#include <sema/sema.h>
#include <cmath>

namespace bloop {

static Struct *analyzingMethodOfStruct = nullptr;

static bool
isComparisonOp(TokenKind kind) {
    switch (kind) {
        case TkEqEq:
        case TkNotEq:
        case TkLt:
        case TkGt:
        case TkLtEq:
        case TkGtEq:
            return true;
        default:
            return false;
    }
}

void
Semantic::analyzeStmt(Stmt *stmt) {
    if (!stmt) {
        return;
    }

    #define NODE(k, f, t) case k: return f(static_cast<t *>(stmt));
    switch (stmt->GetKind()) {
        NODE(NkVarDeclStmt, analyzeVDS, VarDeclStmt);
        NODE(NkVarAsgnStmt, analyzeVAS, VarAsgnStmt);
        NODE(NkFieldAsgnStmt, analyzeFAS, FieldAsgnStmt);
        NODE(NkDerefAsgnStmt, analyzeDAS, DerefAsgnStmt);
        NODE(NkFuncDeclStmt, analyzeFuncBody, FuncDeclStmt);
        NODE(NkFuncCallStmt, analyzeFCS, FuncCallStmt);
        NODE(NkMethodCallStmt, analyzeMCS, MethodCallStmt);
        NODE(NkUsingStmt, analyzeUS, UsingStmt);
        NODE(NkRetStmt, analyzeRS, RetStmt);
        NODE(NkIfElseStmt, analyzeIES, IfElseStmt);
        NODE(NkForLoopStmt, analyzeFLS, ForLoopStmt);
        NODE(NkBreakStmt, analyzeBS, BreakStmt);
        NODE(NkContinueStmt, analyzeCS, ContinueStmt);
        NODE(NkStructDeclStmt, analyzeSDS, StructDeclStmt);
        NODE(NkImplStmt, analyzeIS, ImplStmt);
        NODE(NkDelStmt, analyzeDS, DelStmt);
        NODE(NkArrayAsgnStmt, analyzeAAS, ArrayAsgnStmt);

        default: {
            _diag.Report(Error, "compiler limitation: statement type is currently unimplemented")
                .SetCode(ErrLimitation)
                .AddSpan(stmt->GetStartLoc(), stmt->GetEndLoc());
        }
    }
    #undef NODE
}

void
Semantic::analyzeVDS(VarDeclStmt *vds) {
    auto exprRes = vds->GetExpr() ? analyzeExpr(vds->GetExpr()) : SemanticResult { Value(Value::Unknown, ValueData(), nullptr, llvm::SMLoc(), llvm::SMLoc()), nullptr };
    Value val = exprRes.Val;
    if (vds->GetType()) {
        resolveType(&vds->GetType());
        if (vds->GetExpr()) {
            exprRes = implicitlyCast(exprRes, &vds->GetType());
        }
    }
    else {
        if (vds->GetExpr() && val.Type) {
            vds->GetType() = val.Type;
        }
        else {
            std::stringstream noteText;
            std::string modifier = vds->IsConst() ? "const " : "var ";
            noteText << '`' << modifier;
            noteText << std::string(vds->GetName().Start.getPointer() - vds->GetStartLoc().getPointer() - modifier.size(), ' ');
            noteText << vds->GetName().Name << ": <type>;` or `";
            noteText << modifier;
            noteText << std::string(vds->GetName().Start.getPointer() - vds->GetStartLoc().getPointer() - modifier.size(), ' ');
            noteText << vds->GetName().Name << " = <expr>;`";
            _diag.Report(Error, "cannot inference type")
                .SetCode(ErrCannotInferenceType)
                .AddSpan(vds->GetStartLoc(), vds->GetEndLoc())
                .AddHelp("replace this line to " + noteText.str());
        }
    }

    auto &top = _vars.top();
    if (auto it = top.Vars.find(vds->GetName().Name); it != top.Vars.end()) {
        _diag.Report(Error, "redefinition of '" + vds->GetName().Name + "'")
            .SetCode(ErrRedefinition)
            .AddSpan(it->second.Name.Start, it->second.Name.End, "previous definition is here")
            .AddSpan(vds->GetName().Start, vds->GetName().End, "redefinition here");
        return;
    }

    Variable var(vds->GetName(), vds->GetType(), vds->IsConst(), vds->GetAccess(), val, _vars.size() == 1 ? Static : Stack, _currentFuncVarCount++);
    if (_vars.size() == 1) {
        _mod->Vars.emplace(vds->GetName().Name, var);
    }
    std::string mangledName = _vars.size() == 1 ? _mod->ToString() + "." + vds->GetName().Name : vds->GetName().Name;
    _builder.CreateVar(mangledName, vds->GetType(), exprRes.HirNode, var.Storage, vds->IsConst());
    createVar(vds->GetName().Name, var);
}

void
Semantic::analyzeVAS(VarAsgnStmt *vas) {
    auto varsCopy = _vars;
    while (!varsCopy.empty()) {
        auto &top = varsCopy.top();
        if (auto it = top.Vars.find(vas->GetName().Name); it != top.Vars.end()) {
            if (it->second.IsConst) {
                _diag.Report(Error, "reassignment of read-only variable '" + it->second.Name.Name + "'")
                    .SetCode(ErrReasgnConst)
                    .AddSpan(vas->GetStartLoc(), vas->GetEndLoc());
            }
            auto res = analyzeExpr(vas->GetExpr());
            res = implicitlyCast(res, &it->second.Type);
            _builder.CreateStore(it->second.Storage, it->second.Index, res.HirNode);
            return;
        }
        varsCopy.pop();
    }
    _diag.Report(Error, "variable is undeclared in this scope")
        .SetCode(ErrUndeclaredVar)
        .AddSpan(vas->GetStartLoc(), vas->GetEndLoc(), "undeclared");
}

void
Semantic::analyzeFAS(FieldAsgnStmt *fas) {
    auto baseRes = analyzeExpr(fas->GetBase());

    while (baseRes.Val.Type->IsPointer()) {
        baseRes = ensureSafePointer(baseRes);
        Type *ptrBaseType = baseRes.Val.Type->AsPointer()->GetBaseType();
        baseRes.HirNode = _builder.CreateDereference(baseRes.HirNode, ptrBaseType);
        baseRes.Val.Type = ptrBaseType;
        baseRes.Val.IsLValue = true;
    }
    
    if (baseRes.Val.Type->IsModulePtr()) {
        Module *mod = baseRes.Val.Type->AsModulePtr()->GetMod();
        if (auto it = mod->Vars.find(fas->GetName().Name); it != mod->Vars.end()) {
            if (it->second.Access != Pub) {
                _diag.Report(Error, "symbol '" + fas->GetName().Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(fas->GetName().Start, fas->GetName().End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make variable '" + fas->GetName().Name + "' accessible")
                    .AddHelp("consider using a public method or API instead");
            }
            if (it->second.IsConst) {
                _diag.Report(Error, "reassignment of read-only variable '" + it->second.Name.Name + "'")
                    .SetCode(ErrReasgnConst)
                    .AddSpan(fas->GetStartLoc(), fas->GetEndLoc());
            }
            auto res = analyzeExpr(fas->GetExpr());
            res = implicitlyCast(res, &it->second.Type);
            _builder.CreateStore(it->second.Storage, it->second.Index, res.HirNode);
            return;
        }
        _diag.Report(Error, "symbol '" + fas->GetName().Name + "' is undeclared in module '" + mod->Name + "'")
            .SetCode(ErrUndeclaredSymbol)
            .AddSpan(fas->GetName().Start, fas->GetName().End, "undeclared");
        return;
    }
    else if (baseRes.Val.Type->IsStruct()) {
        auto *st = baseRes.Val.Type->AsStruct();
        auto &s = st->GetBaseMod()->Structs.at(st->GetName().Name);
        auto it = std::find_if(s.Fields.begin(), s.Fields.end(), [&](const Field &f) {
            return f.Var.Name.Name == fas->GetName().Name;
        });
        if (it == s.Fields.end()) {
            _diag.Report(Error, "symbol '" + fas->GetName().Name + "' is undeclared in struct '" + s.Name.Name + "'")
                .SetCode(ErrUndeclaredSymbol)
                .AddSpan(fas->GetName().Start, fas->GetName().End, "undeclared");
            return;
        }
        if (it->Access != Pub) {
            _diag.Report(Error, "symbol '" + fas->GetName().Name + "' is private")
                .SetCode(ErrPrivateSymbol)
                .AddSpan(fas->GetName().Start, fas->GetName().End, "private symbol")
                .AddHelp("consider using the 'pub' keyword to make field '" + fas->GetName().Name + "' accessible")
                .AddHelp("consider using a public method or API instead");
            return;
        }
        if (it->Var.IsConst) {
            _diag.Report(Error, "reassignment of read-only field '" + it->Var.Name.Name + "'")
                .SetCode(ErrReasgnConst)
                .AddSpan(fas->GetStartLoc(), fas->GetEndLoc());
        }
        if (it->IsStatic && baseRes.Val.Kind != Value::TypeLit) {
            _diag.Report(Error, "static field '" + fas->GetName().Name + "' cannot be accessed via an instance")
                .SetCode(ErrAccessStaticFromInstance)
                .AddSpan(fas->GetName().Start, fas->GetName().End, "static symbol")
                .AddHelp("consider using the type name instead: '" + baseRes.Val.Type->ToString() + '.' + fas->GetName().Name + "'");
            return;
        }
        else if (!it->IsStatic && baseRes.Val.Kind == Value::TypeLit) {
            _diag.Report(Error, "non-static field '" + fas->GetName().Name + "' cannot be accessed via a type")
                .SetCode(ErrAccessNonStaticFromType)
                .AddSpan(fas->GetName().Start, fas->GetName().End, "non-static symbol")
                .AddHelp("consider using an instance of '" + baseRes.Val.Type->ToString() + "' or making the field static");
            return;
        }

        auto res = analyzeExpr(fas->GetExpr());
        res = implicitlyCast(res, &it->Var.Type);
        if (!it->IsStatic) {
            int index = 0;
            for (auto &f : s.Fields) {
                if (f == *it) {
                    break;
                }
                if (!f.IsStatic) {
                    ++index;
                }
            }
            
            _builder.CreateStoreField(baseRes.HirNode, baseRes.Val.Type, index, res.HirNode);
        }
        else {
            _builder.CreateStore(Static, _staticFields[s.GetMangledName() + "." + it->Var.Name.Name], res.HirNode);
        }
        return;
    }
    _diag.Report(Error, "symbol '" + fas->GetName().Name + "' is undeclared")
        .SetCode(ErrUndeclaredSymbol)
        .AddSpan(fas->GetName().Start, fas->GetName().End, "undeclared");
}

void
Semantic::analyzeDAS(DerefAsgnStmt *das) {
    auto ptr = analyzeExpr(llvm::cast<DerefExpr>(das->GetBase())->GetBase());
    ptr = ensureSafePointer(ptr);

    auto val = analyzeExpr(das->GetExpr());
    Type *expected = ptr.Val.Type->AsPointer()->GetBaseType();
    val = implicitlyCast(val, &expected);

    _builder.CreateDerefStore(ptr.HirNode, val.HirNode);
}

void
Semantic::analyzeFDS(FuncDeclStmt *fds) {
    if (fds->GetRetType()) {
        resolveType(&fds->GetRetType());
    }
    for (auto &a : fds->GetArgs()) {
        resolveType(&a.Type);
    }
    
    FuncOverload *candidates = findFuncCandidates(fds->GetName().Name);
    if (candidates) {
        for (auto &f : candidates->Candidates) {
            int coincidences = 0;
            if (f.Args.size() == fds->GetArgs().size()) {
                for (int i = 0; i < f.Args.size(); ++i) {
                    if (f.Args[i].Type == fds->GetArgs()[i].Type) {
                        ++coincidences;
                    }
                }
            }
            else {
                continue;
            }
            if (coincidences == fds->GetArgs().size()) {
                std::stringstream ss;
                ss << '(';
                for (int i = 0; i < fds->GetArgs().size(); ++i) {
                    ss << fds->GetArgs()[i].Type->ToString();
                    if (i < fds->GetArgs().size() - 1) {
                        ss << ", ";
                    }
                }
                ss << ')';
                _diag.Report(Error, "redefinition of '" + fds->GetName().Name + ss.str() + "'")
                    .SetCode(ErrRedefinition)
                    .AddSpan(f.Name.Start, f.Name.End, "previous definition is here")
                    .AddSpan(fds->GetName().Start, fds->GetName().End, "redefinition here");
                return;
            }
        }
    }
    else {
        _mod->FuncOverloads.emplace(fds->GetName().Name, FuncOverload());
        candidates = &_mod->FuncOverloads.at(fds->GetName().Name);
    }

    Function func(fds->GetName(), fds->GetRetType(), fds->GetArgs(), fds->GetAccess(), Static, _mod);
    candidates->Candidates.push_back(func);
    std::vector<HIRFuncArgument> hirArgs;
    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        auto &a = fds->GetArgs()[i];
        hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
    }
    
    auto *funcHir = _builder.CreateFunc(func.GetMangledName(), fds->GetRetType(), hirArgs, fds->GetName().Name == "main");
    auto *entry = _builder.CreateBlock(funcHir, "entry");
    _builder.SetInsertPoint(entry);

    _funcsRetTypes.push(fds->GetRetType());
    _vars.push({});

    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        auto &a = fds->GetArgs()[i];
        createVar(a.Name.Name, Variable(a.Name, a.Type, false, Priv, Value::GetIncorrectValue(), Parameter, i));
    }
    
    bool hasRet = false;
    for (auto &s : fds->GetBody()) {
        if (s->GetKind() == NkRetStmt) {
            hasRet = true;
        }
        analyzeStmt(s);
    }
    _vars.pop();
    fds->GetRetType() = _funcsRetTypes.top(); // if type was inferred, then should apply this change
    candidates->Candidates[candidates->Candidates.size() - 1].RetType = fds->GetRetType();
    funcHir->GetRetType() = fds->GetRetType();
    _funcsRetTypes.pop();

    if (!hasRet && fds->GetRetType() && !fds->GetRetType()->IsNothType()) {
        _diag.Report(Error, "function must return a value in all execution paths")
            .SetCode(ErrHasntRet)
            .AddSpan(fds->GetName().Start, fds->GetName().End);
    }
    else if (!hasRet && (!fds->GetRetType() || fds->GetRetType() && fds->GetRetType()->IsNothType())) {
        _builder.CreateRet(new NothType(llvm::SMLoc(), llvm::SMLoc()), nullptr);
    }

    if (fds->GetName().Name == "main") {
        bool correctRetType = !fds->GetRetType() || fds->GetRetType() && fds->GetRetType()->IsInteger() && fds->GetRetType()->AsInteger()->GetBitWidth() == 32;
        bool correctTypesOfArgs = fds->GetArgs().size() == 0 || fds->GetArgs().size() == 2 && fds->GetArgs()[0].Type->IsInteger() &&
                                  fds->GetArgs()[1].Type->IsPointer() &&
                                  fds->GetArgs()[1].Type->AsPointer()->GetBaseType()->IsPointer() &&
                                  fds->GetArgs()[1].Type->AsPointer()->GetBaseType()->AsPointer()->GetBaseType()->IsChar();

        if (!correctRetType || !correctTypesOfArgs) {
            if (!correctRetType) {
                _diag.Report(Error, "invalid signature for function 'main'")
                    .SetCode(ErrInvalidMainFuncSig)
                    .AddSpan(fds->GetName().Start, fds->GetName().End)
                    .AddHelp("try 'func main()' or 'func main(argc: i32, argv: **char): i32'")
                    .AddSpan(fds->GetRetType()->GetStartLoc(), fds->GetRetType()->GetEndLoc(), "expected 'i32' or 'noth'");
            }
            if (!correctTypesOfArgs && fds->GetArgs().size() != 0) {
                _diag.Report(Error, "invalid signature for function 'main'")
                    .SetCode(ErrInvalidMainFuncSig)
                    .AddSpan(fds->GetName().Start, fds->GetName().End)
                    .AddHelp("try 'func main()' or 'func main(argc: i32, argv: **char): i32'")
                    .AddSpan(fds->GetArgs().front().Name.Start, fds->GetArgs().back().Type->GetEndLoc(), "expected nothing or 'i32, **char'");
            }
        }
    }
}

void
Semantic::registerFunc(FuncDeclStmt *fds) {
    std::string name = fds->GetName().Name;
    FuncOverload *candidates = findFuncCandidates(name);
    if (candidates) {
        for (auto &f : candidates->Candidates) {
            int coincidences = 0;
            if (f.Args.size() == fds->GetArgs().size()) {
                for (int i = 0; i < f.Args.size(); ++i) {
                    resolveType(&f.Args[i].Type);
                    resolveType(&fds->GetArgs()[i].Type);
                    if (*f.Args[i].Type == *fds->GetArgs()[i].Type) {
                        ++coincidences;
                    }
                }
            }
            else {
                continue;
            }
            if (coincidences == fds->GetArgs().size()) {
                std::stringstream ss;
                ss << '(';
                for (int i = 0; i < fds->GetArgs().size(); ++i) {
                    ss << fds->GetArgs()[i].Type->ToString();
                    if (i < fds->GetArgs().size() - 1) {
                        ss << ", ";
                    }
                }
                ss << ')';
                _diag.Report(Error, "redefinition of '" + fds->GetName().Name + ss.str() + "'")
                    .SetCode(ErrRedefinition)
                    .AddSpan(f.Name.Start, f.Name.End, "previous definition is here")
                    .AddSpan(fds->GetName().Start, fds->GetName().End, "redefinition here");
                return;
            }
        }
    }
    else {
        _mod->FuncOverloads.emplace(name, FuncOverload());
        candidates = &_mod->FuncOverloads.at(name);
    }

    Function func(fds->GetName(), resolveType(&fds->GetRetType()), fds->GetArgs(), fds->GetAccess(), Static, _mod);
    func.ASTNode = fds;
    func.Status = NotAnalyzed;
    candidates->Candidates.push_back(func);
}

void
Semantic::resolveFuncSignature(Function *func) {
    if (func->Status == AnalysisStatus::SignatureReady || func->Status == AnalysisStatus::BodyAnalyzed) {
        return;
    }
    
    func->Status = ResolvingSig;
    FuncDeclStmt *fds = func->ASTNode;

    resolveType(&fds->GetRetType());
    func->RetType = fds->GetRetType();

    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        auto &a = fds->GetArgs()[i];
        resolveType(&a.Type);
        func->Args[i].Type = a.Type;
    }

    std::vector<HIRFuncArgument> hirArgs;
    for (auto &a : func->Args) {
        hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, nullptr));
    }
    
    func->HirNode = static_cast<HIRFuncDeclStmt *>(_builder.CreateFunc(func->GetMangledName(), func->RetType, hirArgs, fds->GetName().Name == "main"));
    func->Status = SignatureReady;
}

void
Semantic::analyzeFuncBody(FuncDeclStmt *fds) {
    FuncOverload *candidates = findFuncCandidates(fds->GetName().Name);
    Function *func = nullptr;
    
    for (auto &c : candidates->Candidates) {
        if (c.ASTNode == fds) {
            func = &c;
            break;
        }
    }
    
    if (!func || func->Status == BodyAnalyzed) {
        return;
    }

    resolveFuncSignature(func);

    unsigned oldVarCount = _currentFuncVarCount;
    _currentFuncVarCount = 0;
    _funcsRetTypes.push(func->RetType);
    _vars.push({});

    for (int i = 0; i < func->Args.size(); ++i) {
        auto &a = func->Args[i];
        createVar(a.Name.Name, Variable(a.Name, a.Type, false, Priv, Value::GetIncorrectValue(), Parameter, _currentFuncVarCount++));
    }

    auto *entry = _builder.CreateBlock(func->HirNode, "entry");
    _builder.SetInsertPoint(entry);

    bool hasRet = false;
    for (auto &s : fds->GetBody()) {
        if (s->GetKind() == NkRetStmt) {
            hasRet = true;
        }
        analyzeStmt(s);
    }

    _vars.pop();
    _currentFuncVarCount = oldVarCount;
    func->RetType = fds->GetRetType();
    func->HirNode->GetRetType() = fds->GetRetType();

    if (!hasRet && fds->GetRetType() && !fds->GetRetType()->IsNothType()) {
        _diag.Report(Error, "function must return a value in all execution paths")
            .SetCode(ErrHasntRet)
            .AddSpan(fds->GetName().Start, fds->GetName().End);
    }
    else if (!hasRet && (!fds->GetRetType() || fds->GetRetType()->IsNothType())) {
        _builder.CreateRet(new NothType(llvm::SMLoc(), llvm::SMLoc()), nullptr);
    }

    if (fds->GetName().Name == "main") {
        bool correctRetType = !fds->GetRetType() || fds->GetRetType() && fds->GetRetType()->IsInteger() && fds->GetRetType()->AsInteger()->GetBitWidth() == 32;
        bool correctTypesOfArgs = fds->GetArgs().size() == 0 || fds->GetArgs().size() == 2 && fds->GetArgs()[0].Type->IsInteger() &&
                                  fds->GetArgs()[1].Type->IsPointer() &&
                                  fds->GetArgs()[1].Type->AsPointer()->GetBaseType()->IsPointer() &&
                                  fds->GetArgs()[1].Type->AsPointer()->GetBaseType()->AsPointer()->GetBaseType()->IsChar();

        if (!correctRetType || !correctTypesOfArgs) {
            auto err = _diag.Report(Error, "invalid signature for function 'main'");
            err.SetCode(ErrInvalidMainFuncSig)
                .AddHelp("try 'func main()' or 'func main(argc: i32, argv: **char): i32'");
            if (!correctRetType) {
                err.AddSpan(fds->GetRetType()->GetStartLoc(), fds->GetRetType()->GetEndLoc(), "expected 'i32' or 'noth'");
            }
            if (!correctTypesOfArgs && fds->GetArgs().size() != 0) {
                err.AddSpan(fds->GetArgs().front().Name.Start, fds->GetArgs().back().Type->GetEndLoc(), "expected nothing or 'i32, **char'");
            }
        }
    }
    
    func->Status = BodyAnalyzed;
    _funcsRetTypes.pop();
}

void
Semantic::analyzeFCS(FuncCallStmt *fcs) {
    auto res = analyzeExpr(fcs->GetFCE());
    _builder.AddToBlock(res.HirNode);
}

void
Semantic::analyzeMCS(MethodCallStmt *mcs) {
    auto res = analyzeExpr(mcs->GetMCE());
    _builder.AddToBlock(res.HirNode);
}

void
Semantic::analyzeUS(UsingStmt *us) {
    auto chain = us->GetPath();

    FileNode *node = nullptr;
    std::string matchPrefix = "";
    size_t matchIdx = 0;

    for (int i = chain.size(); i > 0; --i) {
        std::string currentPrefix = chain[0].Name;
        for (int j = 1; j < i; ++j) {
            currentPrefix += "." + chain[j].Name;
        }

        if (_graph.count(currentPrefix)) {
            node = const_cast<FileNode *>(&_graph.at(currentPrefix));
            matchPrefix = currentPrefix;
            matchIdx = i;
            break;
        }
    }

    if (matchIdx == 0 || !node) {
        _diag.Report(Error, "module or package not found: '" + chain[0].Name + "'")
            .SetCode(ErrModNotFound)
            .AddSpan(chain[0].Start, chain.back().End);
        return; 
    }

    Module *rootMod = nullptr;
    Module *parentMod = nullptr;

    for (size_t i = 0; i < matchIdx; ++i) {
        const std::string &segmentName = chain[i].Name;
        Module *currentSegmentMod = nullptr;

        if (i == 0) {
            if (_mod->Imports.count(segmentName)) {
                currentSegmentMod = _mod->Imports[segmentName];
            }
        }
        else if (parentMod && parentMod->Submods.count(segmentName)) {
            currentSegmentMod = parentMod->Submods[segmentName];
        }

        if (!currentSegmentMod) {
            if (i == matchIdx - 1) {
                currentSegmentMod = node->Mod;
            }
            else {
                currentSegmentMod = new Module(segmentName, Pub, parentMod);
            }

            if (i == 0) {
                _mod->Imports[segmentName] = currentSegmentMod;
            }
            else if (parentMod) {
                parentMod->Submods[segmentName] = currentSegmentMod;
            }
        }

        if (i == 0) {
            rootMod = currentSegmentMod;
        }

        parentMod = currentSegmentMod;
    }

    for (size_t i = matchIdx; i < chain.size(); ++i) {
        const std::string &symName = chain[i].Name;
        
        if (parentMod->Submods.count(symName)) {
            parentMod = parentMod->Submods[symName];
        }
        else {
            _diag.Report(Error, "compiler limitation: selective symbol import is currently unimplemented")
                .SetCode(ErrLimitation)
                .AddSpan(chain[i].Start, chain[i].End)
                .AddNote("currently supports only full module imports.")
                .AddHelp("consider using the full module path: 'using " + parentMod->ToString() + ";' and accessing '" + symName + "' via '" + parentMod->ToString() + "." + symName + "'");
            break;
        }
    }

    _mod->Imports[rootMod->Name] = rootMod;
    for (auto &[name, obj] : node->Mod->Vars) {
        _builder.CreateVar(node->Mod->ToString() + "." + name, obj.Type, nullptr, Extern);
    }
    for (auto &[name, s] : node->Mod->Structs) {
        std::vector<Type *> fields;
        for (auto &f : s.Fields) {
            resolveType(&f.Var.Type);
            fields.push_back(f.Var.Type);
            if (f.IsStatic) {
                _builder.CreateVar(s.GetMangledName() + "." + f.Var.Name.Name, f.Var.Type, nullptr, Extern);
                _staticFields[s.GetMangledName() + "." + f.Var.Name.Name] = _builder.GetContext().GetGlobals().size() - 1;
            }
        }
        _builder.CreateStruct(s.GetMangledName(), fields);

        for (auto &candidates : s.Methods) {
            for (auto &c : candidates.Candidates) {
                std::vector<HIRFuncArgument> hirArgs;
                for (int i = 0; i < c.Func.Args.size(); ++i) {
                    auto &a = c.Func.Args[i];
                    hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
                }
                _builder.CreateFunc(s.GetMangledName() + "." + c.Func.Name.Name, c.Func.RetType, hirArgs, false, true);
            }
        }
    }
    for (auto &[name, candidates] : node->Mod->FuncOverloads) {
        for (auto &c : candidates.Candidates) {
            std::vector<HIRFuncArgument> hirArgs;
            for (int i = 0; i < c.Args.size(); ++i) {
                auto &a = c.Args[i];
                hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
            }
            _builder.CreateFunc(c.GetMangledName(), c.RetType, hirArgs, name == "main", true);
        }
    }
    for (auto &[t, overloads] : node->Mod->PrimitivesMethods) {
        for (auto &candidates : overloads) {
            for (auto &c : candidates.Candidates) {
                std::vector<HIRFuncArgument> hirArgs;
                for (int i = 0; i < c.Func.Args.size(); ++i) {
                    auto &a = c.Func.Args[i];
                    hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
                }
                _builder.CreateFunc(c.Func.Parent->ToString() + "." + t->ToString() + "." + c.Func.Name.Name, c.Func.RetType, hirArgs, false, true);
            }
        }
    }
}

void
Semantic::analyzeRS(RetStmt *rs) {
    if (rs->GetExpr()) {
        auto valRes = analyzeExpr(rs->GetExpr());
        Value val = valRes.Val;
        valRes = implicitlyCast(valRes, &_funcsRetTypes.top());
        _builder.CreateRet(_funcsRetTypes.top(), valRes.HirNode);
    }
    else {
        if (_funcsRetTypes.top()->IsNothType()) {
            _builder.CreateRet(_funcsRetTypes.top(), nullptr);
        }
        else {
            _diag.Report(Error, "cannot implicitly cast 'noth' to '" + _funcsRetTypes.top()->ToString() + "'")
                .SetCode(ErrCannotImplCast)
                .AddSpan(rs->GetStartLoc(), rs->GetEndLoc())
                .AddHelp("сonsider using an explicit cast");
        }
    }
}

void
Semantic::analyzeIES(IfElseStmt *ies) {
    auto thenBody = _builder.CreateBlock(_builder.GetParent(), "cond.then");
    auto elseBody = _builder.CreateBlock(_builder.GetParent(), "cond.else");
    auto mergeBody = _builder.CreateBlock(_builder.GetParent(), "cond.merge");

    auto condRes = analyzeExpr(ies->GetCond());
    Type *boolType = new IntegerType(1, false, condRes.Val.Start, condRes.Val.End);
    condRes = implicitlyCast(condRes, &boolType);
    _builder.CreateBr(condRes.HirNode, thenBody, elseBody);

    _builder.SetInsertPoint(thenBody);
    _vars.push({});
    for (auto &s : ies->GetThenBranch()) {
        analyzeStmt(s);
    }
    _vars.pop();
    _builder.CreateBr(mergeBody);

    _builder.SetInsertPoint(elseBody);
    _vars.push({});
    for (auto &s : ies->GetElseBranch()) {
        analyzeStmt(s);
    }
    _vars.pop();
    _builder.CreateBr(mergeBody);

    _builder.SetInsertPoint(mergeBody);
}

void
Semantic::analyzeFLS(ForLoopStmt *fls) {
    _vars.push({});

    auto indexator = _builder.CreateBlock(_builder.GetParent(), "for.indexator");
    auto cond = _builder.CreateBlock(_builder.GetParent(), "for.cond");
    auto iteration = _builder.CreateBlock(_builder.GetParent(), "for.iteration");
    auto body = _builder.CreateBlock(_builder.GetParent(), "for.body");
    auto exit = _builder.CreateBlock(_builder.GetParent(), "for.exit");
    _builder.CreateBr(indexator);

    _loops.push({ exit, cond });
    
    _builder.SetInsertPoint(indexator);
    if (fls->GetIndexator()) {
        analyzeStmt(fls->GetIndexator());
    }
    _builder.CreateBr(cond);

    _builder.SetInsertPoint(cond);
    auto condRes = analyzeExpr(fls->GetCond());
    Type *boolType = new IntegerType(1, false, condRes.Val.Start, condRes.Val.End);
    condRes = implicitlyCast(condRes, &boolType);
    _builder.CreateBr(condRes.HirNode, body, exit);

    _builder.SetInsertPoint(iteration);
    if (fls->GetIteration()) {
        analyzeStmt(fls->GetIteration());
    }
    _builder.CreateBr(cond);

    _builder.SetInsertPoint(body);
    for (auto &s : fls->GetBody()) {
        analyzeStmt(s);
    }
    _builder.CreateBr(iteration);
    
    _loops.pop();
    
    _vars.pop();

    _builder.SetInsertPoint(exit);
}

void
Semantic::analyzeBS(BreakStmt *bs) {
    if (_loops.empty()) {
        _diag.Report(Error, "cannot 'break' outside of a loop")
            .SetCode(ErrControlFlowOpOutsideLoop)
            .AddSpan(bs->GetStartLoc(), bs->GetEndLoc());
        return;
    }
    _builder.CreateBr(_loops.top().Break);
}

void
Semantic::analyzeCS(ContinueStmt *cs) {
    if (_loops.empty()) {
        _diag.Report(Error, "cannot 'continue' outside of a loop")
            .SetCode(ErrControlFlowOpOutsideLoop)
            .AddSpan(cs->GetStartLoc(), cs->GetEndLoc());
        return;
    }
    _builder.CreateBr(_loops.top().Continue);
}

void
Semantic::analyzeSDS(StructDeclStmt *sds) {
    if (auto it = _mod->Structs.find(sds->GetName().Name); it != _mod->Structs.end()) {
        _diag.Report(Error, "redefinition of '" + sds->GetName().Name + "'")
            .SetCode(ErrRedefinition)
            .AddSpan(it->second.Name.Start, it->second.Name.End, "previous definition is here")
            .AddSpan(sds->GetName().Start, sds->GetName().End, "redefinition here");
        return;
    }

    std::unordered_map<std::string, Field *> fieldsMap;
    std::vector<Field> fields;
    std::vector<Type *> fieldsTypes;
    for (auto &f : sds->GetFields()) {
        if (auto it = fieldsMap.find(f.Name.Name); it != fieldsMap.end()) {
            _diag.Report(Error, "redefinition of '" + f.Name.Name + "'")
                .SetCode(ErrRedefinition)
                .AddSpan(it->second->Var.Name.Start, it->second->Var.Name.End, "previous definition is here")
                .AddSpan(f.Name.Start, f.Name.End, "redefinition here");
            continue;
        }
        if (f.Type->IsUnknownNamedType()) {
            auto *unknown = f.Type->AsUnknownNamedType();
            if (unknown->GetPath().size() == 1 && unknown->GetPath()[0].Name == sds->GetName().Name) {
                _diag.Report(Error, "recursive type '" + sds->GetName().Name + "' has infinite size")
                    .SetCode(ErrRecursiveType)
                    .AddSpan(f.Type->GetStartLoc(), f.Type->GetEndLoc());
                continue;
            }
        }
        resolveType(&f.Type);
        Field field(Variable(f.Name, f.Type, false, f.Access, Value::GetIncorrectValue()), f.IsStatic, f.Access);
        SemanticResult defaultVal;
        if (f.DefaultVal) {
            defaultVal = analyzeExpr(f.DefaultVal);
            defaultVal = implicitlyCast(defaultVal, &f.Type);
        }
        if (f.IsStatic) {
            _builder.CreateVar(_mod->ToString() + "." + sds->GetName().Name + "." + f.Name.Name, f.Type, defaultVal.HirNode, Static);
            _staticFields[_mod->ToString() + "." + sds->GetName().Name + "." + f.Name.Name] = _builder.GetContext().GetGlobals().size() - 1;
        }
        fields.push_back(field);
        fieldsMap.emplace(f.Name.Name, &fields.back());
        if (!f.IsStatic) {
            fieldsTypes.push_back(f.Type);
        }
    }
    
    auto *hirNode = _builder.CreateStruct(_mod->ToString() + "." + sds->GetName().Name, fieldsTypes);
    _mod->Structs.emplace(sds->GetName().Name, Struct(sds->GetName(), _mod, fields, sds->GetAccess()));
}

void
Semantic::registerImplMethods(ImplStmt *is) {
    Type *&type = is->GetStructType();
    resolveType(&type);

    if (is->GetTraitType()) {
        _diag.Report(Error, "compiler limitation: trait implementations are currently unimplemented")
            .SetCode(ErrLimitation)
            .AddSpan(is->GetTraitType()->GetStartLoc(), is->GetTraitType()->GetEndLoc());
        return;
    }

    if (type->IsStruct()) {
        StructType *sType = type->AsStruct();
        Struct *s = &sType->GetBaseMod()->Structs.at(sType->GetName().Name);

        for (auto &m : is->GetMethods()) {
            registerMethod(s, &m);
        }
    }
    else {
        for (auto &m : is->GetMethods()) {
            registerPrimitiveMethod(type, &m);
        }
    }
}

void
Semantic::registerMethod(Struct *s, ImplStmt::Method *method) {
    std::string name = method->Name.Name;
    MethodOverload *overload = nullptr;

    for (auto &o : s->Methods) {
        if (!o.Candidates.empty() && o.Candidates[0].Func.Name.Name == name) {
            overload = &o;
            break;
        }
    }

    if (overload) {
        for (auto &m : overload->Candidates) {
            if (m.Func.Name.Name != name) {
                continue;
            }
            
            int coincidences = 0;
            if (m.Func.Args.size() == method->Args.size()) {
                for (int i = 0; i < m.Func.Args.size(); ++i) {
                    resolveType(&m.Func.Args[i].Type);
                    Type *fdsArgType = method->Args[i].Type;
                    resolveType(&fdsArgType);
                    if (*m.Func.Args[i].Type == *fdsArgType) {
                        ++coincidences;
                    }
                }
            }
            
            if (coincidences == method->Args.size()) {
                std::stringstream ss;
                ss << '(';
                for (int i = 0; i < method->Args.size(); ++i) {
                    ss << method->Args[i].Type->ToString();
                    if (i < method->Args.size() - 1) {
                        ss << ", ";
                    }
                }
                ss << ')';
                
                _diag.Report(Error, "redefinition of method '" + name + ss.str() + "' in struct '" + s->Name.Name + "'")
                    .SetCode(ErrRedefinition)
                    .AddSpan(m.Func.Name.Start, m.Func.Name.End, "previous definition is here")
                    .AddSpan(method->Name.Start, method->Name.End, "redefinition here");
                return;
            }
        }
    }
    else {
        s->Methods.push_back(MethodOverload());
        overload = &s->Methods.back();
    }

    Function func(method->Name, resolveType(&method->RetType), method->Args, method->Access, Static, s->Parent);
    func.ASTNode = nullptr;
    func.Status = NotAnalyzed;
    
    overload->Candidates.push_back(Method(func, method->IsStatic, method->Access));
}

void
Semantic::resolveMethodSignature(Struct *s, Method *method, ImplStmt::Method *methodObj) {
    Function *func = &method->Func;
    if (func->Status == AnalysisStatus::SignatureReady || func->Status == AnalysisStatus::BodyAnalyzed) {
        return;
    }
    
    func->Status = ResolvingSig;
    resolveType(&methodObj->RetType);
    func->RetType = methodObj->RetType;

    for (int i = 0; i < methodObj->Args.size(); ++i) {
        auto &a = methodObj->Args[i];
        resolveType(&a.Type);
        func->Args[i].Type = a.Type;
    }

    std::string mangledName = s->Parent->ToString() + "." + s->Name.Name + "." + func->Name.Name;
    std::vector<HIRFuncArgument> hirArgs;
    if (!method->IsStatic) {
        Type *t = new PointerType(new StructType(s->Name, s->Parent, llvm::SMLoc(), llvm::SMLoc()), llvm::SMLoc(), llvm::SMLoc());
        hirArgs.push_back(HIRFuncArgument("this", t, nullptr));
    }
    for (auto &a : func->Args) {
        hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, nullptr));
        mangledName += a.Type->ToString();
    }
    
    func->HirNode = static_cast<HIRFuncDeclStmt *>(_builder.CreateFunc(mangledName, func->RetType, hirArgs, false));
    func->Status = SignatureReady;
}

void
Semantic::analyzeMethodBody(Struct *s, Method *method, ImplStmt::Method *methodObj) {
    Function *func = &method->Func;

    if (func->Status == BodyAnalyzed) {
        return;
    }

    resolveMethodSignature(s, method, methodObj);

    unsigned oldVarCount = _currentFuncVarCount;
    _currentFuncVarCount = 0;
    _funcsRetTypes.push(func->RetType);
    _vars.push({});

    if (!method->IsStatic) {
        NameObj name("this", methodObj->Name.Start, methodObj->Name.End);
        Type *t = new StructType(s->Name, s->Parent, llvm::SMLoc(), llvm::SMLoc());
        createVar("this", Variable(name, t, false, Priv, Value(Value::This, ValueData(), t, llvm::SMLoc(), llvm::SMLoc()), Parameter, _currentFuncVarCount++));
    }
    for (int i = 0; i < func->Args.size(); ++i) {
        auto &a = func->Args[i];
        createVar(a.Name.Name, Variable(a.Name, a.Type, false, Priv, Value::GetIncorrectValue(), Parameter, _currentFuncVarCount++));
    }

    auto *entry = _builder.CreateBlock(func->HirNode, "entry");
    _builder.SetInsertPoint(entry);

    analyzingMethodOfStruct = s;
    bool hasRet = false;
    for (auto &stmt : methodObj->Body) {
        if (stmt->GetKind() == NkRetStmt) {
            hasRet = true;
        }
        analyzeStmt(stmt);
    }
    analyzingMethodOfStruct = nullptr;

    _vars.pop();
    _currentFuncVarCount = oldVarCount;
    func->RetType = methodObj->RetType;
    func->HirNode->GetRetType() = methodObj->RetType;

    if (!hasRet && methodObj->RetType && !methodObj->RetType->IsNothType()) {
        _diag.Report(Error, "method must return a value in all execution paths")
            .SetCode(ErrHasntRet)
            .AddSpan(methodObj->Name.Start, methodObj->Name.End);
    }
    else if (!hasRet && (!methodObj->RetType || methodObj->RetType->IsNothType())) {
        _builder.CreateRet(new NothType(llvm::SMLoc(), llvm::SMLoc()), nullptr);
    }
    
    func->Status = BodyAnalyzed;
    _funcsRetTypes.pop();
}

void
Semantic::registerPrimitiveMethod(Type *type, ImplStmt::Method *method) {
    std::string name = method->Name.Name;
    std::vector<MethodOverload> *methodsVec = nullptr;

    for (auto &[t, m] : _mod->PrimitivesMethods) {
        if (*t == *type) {
            methodsVec = &m;
            break;
        }
    }

    if (!methodsVec) {
        _mod->PrimitivesMethods[type] = {};
        for (auto &[t, m] : _mod->PrimitivesMethods) {
            if (*t == *type) {
                methodsVec = &m;
                break;
            }
        }
    }

    MethodOverload *overload = nullptr;
    for (auto &o : *methodsVec) {
        if (!o.Candidates.empty() && o.Candidates[0].Func.Name.Name == name) {
            overload = &o;
            break;
        }
    }

    if (overload) {
        for (auto &m : overload->Candidates) {
            if (m.Func.Name.Name != name) {
                continue;
            }
            int coincidences = 0;
            if (m.Func.Args.size() == method->Args.size()) {
                for (int i = 0; i < m.Func.Args.size(); ++i) {
                    resolveType(&m.Func.Args[i].Type);
                    Type *fdsArgType = method->Args[i].Type;
                    resolveType(&fdsArgType);
                    if (*m.Func.Args[i].Type == *fdsArgType) {
                        ++coincidences;
                    }
                }
            }
            if (coincidences == method->Args.size()) {
                std::stringstream ss;
                ss << '(';
                for (int i = 0; i < method->Args.size(); ++i) {
                    ss << method->Args[i].Type->ToString();
                    if (i < method->Args.size() - 1) {
                        ss << ", ";
                    }
                }
                ss << ')';
                
                _diag.Report(Error, "redefinition of method '" + name + ss.str() + "' in type '" + type->ToString() + "'")
                    .SetCode(ErrRedefinition)
                    .AddSpan(m.Func.Name.Start, m.Func.Name.End, "previous definition is here")
                    .AddSpan(method->Name.Start, method->Name.End, "redefinition here");
                return;
            }
        }
    }
    else {
        methodsVec->push_back(MethodOverload());
        overload = &methodsVec->back();
    }

    Function func(method->Name, resolveType(&method->RetType), method->Args, method->Access, Static, _mod);
    func.ASTNode = nullptr;
    func.Status = NotAnalyzed;
    
    overload->Candidates.push_back(Method(func, method->IsStatic, method->Access));
}

void
Semantic::resolvePrimitiveMethodSignature(Type *type, Method *method, ImplStmt::Method *methodObj) {
    Function *func = &method->Func;
    if (func->Status == AnalysisStatus::SignatureReady || func->Status == AnalysisStatus::BodyAnalyzed) {
        return;
    }
    
    func->Status = ResolvingSig;
    resolveType(&methodObj->RetType);
    func->RetType = methodObj->RetType;

    for (int i = 0; i < methodObj->Args.size(); ++i) {
        auto &a = methodObj->Args[i];
        resolveType(&a.Type);
        func->Args[i].Type = a.Type;
    }

    std::string mangledName = _mod->ToString() + "." + type->ToString() + "." + func->Name.Name;
    std::vector<HIRFuncArgument> hirArgs;
    
    if (!method->IsStatic) {
        Type *thisType = new PointerType(type, llvm::SMLoc(), llvm::SMLoc());
        hirArgs.push_back(HIRFuncArgument("this", thisType, nullptr));
    }
    
    for (auto &a : func->Args) {
        hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, nullptr));
        mangledName += a.Type->ToString();
    }
    
    func->HirNode = static_cast<HIRFuncDeclStmt *>(_builder.CreateFunc(mangledName, func->RetType, hirArgs, false));
    func->Status = SignatureReady;
}

void
Semantic::analyzePrimitiveMethodBody(Type *type, Method *method, ImplStmt::Method *methodObj) {
    Function *func = &method->Func;

    if (func->Status == BodyAnalyzed) {
        return;
    }

    resolvePrimitiveMethodSignature(type, method, methodObj);

    unsigned oldVarCount = _currentFuncVarCount;
    _currentFuncVarCount = 0;
    _funcsRetTypes.push(func->RetType);
    _vars.push({});

    if (!method->IsStatic) {
        NameObj name("this", methodObj->Name.Start, methodObj->Name.End);
        Type *thisType = new PointerType(type, llvm::SMLoc(), llvm::SMLoc());
        createVar("this", Variable(name, thisType, false, Priv, Value(Value::This, ValueData(), thisType, llvm::SMLoc(), llvm::SMLoc()), Parameter, _currentFuncVarCount++));
    }
    for (int i = 0; i < func->Args.size(); ++i) {
        auto &a = func->Args[i];
        createVar(a.Name.Name, Variable(a.Name, a.Type, false, Priv, Value::GetIncorrectValue(), Parameter, _currentFuncVarCount++));
    }

    auto *entry = _builder.CreateBlock(func->HirNode, "entry");
    _builder.SetInsertPoint(entry);

    bool hasRet = false;
    for (auto &stmt : methodObj->Body) {
        if (stmt->GetKind() == NkRetStmt) {
            hasRet = true;
        }
        analyzeStmt(stmt);
    }

    _vars.pop();
    _currentFuncVarCount = oldVarCount;
    func->RetType = methodObj->RetType;
    func->HirNode->GetRetType() = methodObj->RetType;

    if (!hasRet && methodObj->RetType && !methodObj->RetType->IsNothType()) {
        _diag.Report(Error, "method must return a value in all execution paths")
            .SetCode(ErrHasntRet)
            .AddSpan(methodObj->Name.Start, methodObj->Name.End);
    }
    else if (!hasRet && (!methodObj->RetType || methodObj->RetType->IsNothType())) {
        _builder.CreateRet(new NothType(llvm::SMLoc(), llvm::SMLoc()), nullptr);
    }
    
    func->Status = BodyAnalyzed;
    _funcsRetTypes.pop();
}

void
Semantic::analyzeIS(ImplStmt *is) {
    Type *type = is->GetStructType();
    
    if (is->GetTraitType()) {
        _diag.Report(Error, "compiler limitation: trait implementations are currently unimplemented")
            .SetCode(ErrLimitation)
            .AddSpan(is->GetTraitType()->GetStartLoc(), is->GetTraitType()->GetEndLoc());
        return;
    }

    if (type->IsStruct()) {
        StructType *sType = type->AsStruct(); 
        Struct *s = &sType->GetBaseMod()->Structs.at(sType->GetName().Name); 
        
        for (int i = 0; i < is->GetMethods().size(); ++i) {
            ImplStmt::Method *method = &is->GetMethods()[i];
            MethodOverload *overload = nullptr;
            
            for (auto &o : s->Methods) {
                if (!o.Candidates.empty() && o.Candidates[0].Func.Name.Name == method->Name.Name) {
                    overload = &o;
                    break;
                }
            }
            
            if (overload) {
                auto it = std::find_if(overload->Candidates.begin(), overload->Candidates.end(), [&](const Method &m) {
                    if (m.Func.Name.Name != method->Name.Name) {
                        return false;
                    }
                    if (m.Func.Args.size() != method->Args.size()) {
                        return false;
                    }
                    for (int j = 0; j < m.Func.Args.size(); ++j) {
                        if (*m.Func.Args[j].Type != *method->Args[j].Type) {
                            return false; 
                        }
                    }
                    return true;
                });

                if (it != overload->Candidates.end()) {
                    analyzeMethodBody(s, &(*it), method);
                }
            }
        }
    }
    else {
        for (int i = 0; i < is->GetMethods().size(); ++i) {
            ImplStmt::Method *method = &is->GetMethods()[i];
            MethodOverload *overload = findPrimitiveMethodCandidates(type, method->Name.Name);
            
            if (overload) {
                auto it = std::find_if(overload->Candidates.begin(), overload->Candidates.end(), [&](const Method &m) {
                    if (m.Func.Name.Name != method->Name.Name) {
                        return false;
                    }
                    if (m.Func.Args.size() != method->Args.size()) {
                        return false;
                    }
                    for (int j = 0; j < m.Func.Args.size(); ++j) {
                        if (*m.Func.Args[j].Type != *method->Args[j].Type) {
                            return false; 
                        }
                    }
                    return true;
                });

                if (it != overload->Candidates.end()) {
                    analyzePrimitiveMethodBody(type, &(*it), method);
                }
            }
        }
    }
}

void
Semantic::analyzeDS(DelStmt *ds) {
    // TODO: mark base pointer object as Value::Nil
    auto res = analyzeExpr(ds->GetExpr());
    if (!res.Val.Type->IsPointer()) {
        _diag.Report(Error, "operator 'del' requires a pointer type; found '" + res.Val.Type->ToString() + "'")
            .SetCode(ErrDelForNonPtrObj)
            .AddSpan(ds->GetExpr()->GetStartLoc(), ds->GetExpr()->GetEndLoc());
        return;
    }
    auto *baseType = res.Val.Type->AsPointer();
    if (auto *slice = baseType->GetBaseType()->AsSlice()) {
        auto *sliceVal = _builder.CreateDereference(res.HirNode, slice);
        auto *data = _builder.CreateFieldExpr(sliceVal, slice->GetBaseType(), 0);
        _builder.CreateDel(data);
    }
    _builder.CreateDel(res.HirNode);
}

void
Semantic::analyzeAAS(ArrayAsgnStmt *aas) {
    auto baseRes = analyzeExpr(aas->GetBase());
    auto indexRes = analyzeExpr(aas->GetIndex());

    if (!indexRes.Val.Type->IsSizeType()) {
        _diag.Report(Error, "array index must be an usize")
            .SetCode(ErrCannotImplCast)
            .AddSpan(aas->GetIndex()->GetStartLoc(), aas->GetIndex()->GetEndLoc());
        return;
    }

    Type *resultType = nullptr;
    HIRNode *hirBase = baseRes.HirNode;

    auto mgr = _diag.GetSourceMgr();
    auto &buff = mgr->getBufferInfo(mgr->FindBufferContainingLoc(aas->GetIndex()->GetStartLoc()));
    auto lineAndCol = mgr->getLineAndColumn(aas->GetIndex()->GetStartLoc());
    std::string pos = buff.Buffer->getBufferIdentifier().str() + ":" + std::to_string(lineAndCol.first) + ":" + std::to_string(lineAndCol.second);
    
    if (baseRes.Val.Type->IsArray()) {
        resultType = baseRes.Val.Type->AsArray()->GetBaseType();
        // TODO: add bounds check node
    }
    else if (baseRes.Val.Type->IsSlice()) {
        resultType = baseRes.Val.Type->AsSlice()->GetBaseType();
        hirBase = _builder.CreateFieldExpr(hirBase, baseRes.Val.Type, 0);
        HIRNode *len = _builder.CreateFieldExpr(baseRes.HirNode, baseRes.Val.Type, 1);
        indexRes.HirNode = _builder.CreateBoundsCheck(len, indexRes.HirNode, pos);
    }
    else {
        _diag.Report(Error, "type '" + baseRes.Val.Type->ToString() + "' does not support indexing")
            .SetCode(ErrCannotApplyOp)
            .AddSpan(aas->GetBase()->GetStartLoc(), aas->GetBase()->GetEndLoc());
        return;
    }

    auto exprRes = analyzeExpr(aas->GetExpr());
    exprRes = implicitlyCast(exprRes, &resultType);

    _builder.CreateArrayStore(hirBase, baseRes.Val.Type, indexRes.HirNode, exprRes.HirNode);
}

Semantic::SemanticResult
Semantic::analyzeExpr(Expr *expr) {
    #define NODE(k, f, t) case k: return f(llvm::cast<t>(expr));
    switch (expr->GetKind()) {
        NODE(NkBinaryExpr, analyzeBE, BinaryExpr);
        NODE(NkLitExpr, analyzeLE, LiteralExpr);
        NODE(NkUnaryExpr, analyzeUE, UnaryExpr);
        NODE(NkVarExpr, analyzeVE, VarExpr);
        NODE(NkFuncCallExpr, analyzeFCE, FuncCallExpr);
        NODE(NkFieldExpr, analyzeFE, FieldExpr);
        NODE(NkMethodCallExpr, analyzeMCE, MethodCallExpr);
        NODE(NkStructInstanceExpr, analyzeSIE, StructInstanceExpr);
        NODE(NkTypeExpr, analyzeTE, TypeExpr);
        NODE(NkNilExpr, analyzeNE, NilExpr);
        NODE(NkRefExpr, analyzeRE, RefExpr);
        NODE(NkDerefExpr, analyzeDE, DerefExpr);
        NODE(NkNewExpr, analyzeNew, NewExpr);
        NODE(NkArrayInstanceExpr, analyzeAIE, ArrayInstanceExpr);
        NODE(NkArrayAccessExpr, analyzeAAE, ArrayAccessExpr);
        
        default: {
            _diag.Report(Error, "compiler limitation: expression type is currently unimplemented")
                .SetCode(ErrLimitation)
                .AddSpan(expr->GetStartLoc(), expr->GetEndLoc());
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }
    }
    #undef NODE
}

Semantic::SemanticResult
Semantic::analyzeBE(BinaryExpr *be) {
    auto lhsRes = analyzeExpr(be->GetLHS());
    auto rhsRes = analyzeExpr(be->GetRHS());
    Value lhs = lhsRes.Val;
    Value rhs = rhsRes.Val;
    resolveType(&lhs.Type);
    resolveType(&rhs.Type);
    Type *commonType = getCommonTypeForOp(lhs.Type, rhs.Type, be->GetOp(), be->GetStartLoc(), be->GetEndLoc());
    lhsRes = implicitlyCast(lhsRes, &commonType);
    rhsRes = implicitlyCast(rhsRes, &commonType);

    Type *resultType = nullptr;
    if (isComparisonOp(be->GetOp().Kind)) {
        resultType = new IntegerType(1, false, be->GetStartLoc(), be->GetEndLoc());
    }
    else {
        resultType = commonType;
    }

    HIRNode *binNode = _builder.CreateBinary(commonType, lhsRes.HirNode, rhsRes.HirNode, tokenKindToHIRBk(be->GetOp().Kind));

    if (lhs.IsUnknown() || rhs.IsUnknown()) {
        return { Value(Value::Unknown, ValueData(), resultType, be->GetStartLoc(), be->GetEndLoc()), binNode };
    }

    // TODO: add suporting of strings
    double lhsVal;
    double rhsVal;
    if (lhs.Type->IsInteger() || lhs.Type->IsSizeType()) {
        lhsVal = std::get<0>(lhs.Data);
    }
    else {
        lhsVal = std::get<1>(lhs.Data);
    }
    if (rhs.Type->IsInteger() || rhs.Type->IsSizeType()) {
        rhsVal = std::get<0>(rhs.Data);
    }
    else {
        rhsVal = std::get<1>(rhs.Data);
    }
    double res = 0;
    switch (be->GetOp().Kind) {
        #define CASE(n, op) case n: res = lhsVal op rhsVal; break;
        CASE(TkPlus, +)
        CASE(TkMinus, -)
        CASE(TkStar, *)
        CASE(TkSlash, /)
        case TkPercent:
            res = fmod(lhsVal, rhsVal);
            break;
        CASE(TkLt, <)
        CASE(TkGt, >)
        CASE(TkLtEq, <=)
        CASE(TkGtEq, >=)
        CASE(TkEqEq, ==)
        CASE(TkNotEq, !=)
        CASE(TkLogAnd, &&)
        CASE(TkLogOr, ||)
        #undef CASE
    }
    
    switch (resultType->GetKind()) {
        #define VAL(t) Value(Value::Const, ValueData(static_cast<t>(res)), resultType, be->GetStartLoc(), be->GetEndLoc())
        case Type::Integer:
        case Type::Size:
            return { VAL(int64_t), _builder.CreateLiteral(VAL(int64_t)) };
        case Type::Floating:
            return { VAL(double), _builder.CreateLiteral(VAL(double)) };
        // TODO: add suporting of strings
        #undef VAL
    }
}

Semantic::SemanticResult
Semantic::analyzeLE(LiteralExpr *le) {
    auto val = le->GetVal();
    if (val.Type->IsString()) {
        const std::string &str = std::get<std::string>(val.Data);
        int64_t len = str.length();

        auto *u8Type = new IntegerType(8, true, val.Start, val.End);
        auto *lenType = new IntegerType(64, true, val.Start, val.End);

        auto *sliceType = new SliceType(u8Type, val.Start, val.End);

        HIRNode *ptrNode = _builder.CreateLiteral(val);

        Value lenVal(Value::Const, (int64_t)len, lenType, val.Start, val.End);
        HIRNode *lenNode = _builder.CreateLiteral(lenVal);

        std::vector<std::pair<int, HIRNode *>> fields = {
             { 0, ptrNode },
             { 1, lenNode }
        };
        std::vector<Type *> sliceFields = {
            new PointerType(u8Type, llvm::SMLoc(), llvm::SMLoc()),
            new SizeType(true, llvm::SMLoc(), llvm::SMLoc())
        };
        _builder.CreateStruct("slice." + u8Type->ToString(), sliceFields);
        HIRNode *sliceNode = _builder.CreateStructInstance("slice." + u8Type->ToString(), fields);

        return SemanticResult(Value(Value::Const, val.Data, sliceType, val.Start, val.End), sliceNode);
    }
    return { le->GetVal(), _builder.CreateLiteral(le->GetVal()) };
}

Semantic::SemanticResult
Semantic::analyzeUE(UnaryExpr *ue) {
    auto rhsRes = analyzeExpr(ue->GetRHS());
    Value rhs = rhsRes.Val;
    resolveType(&rhs.Type);

    bool ok = true;
    switch (ue->GetOp().Kind) {
        case TkMinus:
            if (!rhs.Type->IsNumber()) {
                ok = false;
                _diag.Report(Error, "cannot apply operator '" + ue->GetOp().Val + "' to type '" + rhs.Type->ToString() + "'")
                    .SetCode(ErrCannotApplyOp)
                    .AddSpan(ue->GetStartLoc(), ue->GetEndLoc());
            }
            else if (auto *rhsT = rhs.Type->AsInteger()) {
                if (rhsT->IsUnsigned() && rhs.Kind == Value::Const) {
                    ok = false;
                    int64_t litVal = std::get<0>(rhs.Data);
                    _diag.Report(Warning, "unsigned conversion of -" + std::to_string(litVal) + " yields the maximum value of the type")
                        .SetCode(WarnLiteralUnderflow)
                        .AddSpan(ue->GetStartLoc(), ue->GetEndLoc(), "this operation will return the value " + std::to_string((1ULL << rhsT->GetBitWidth()) - 1 - litVal))
                        .AddHelp("remove suffix of number literal or '-' operator");
                }
            }
            break;
        case TkBang:
            if (rhs.Type->IsInteger()) {
                auto *rhsT = rhs.Type->AsInteger();
                if (rhsT->GetBitWidth() != 1) {
                    ok = false;
                    _diag.Report(Error, "cannot apply operator '" + ue->GetOp().Val + "' to type '" + rhs.Type->ToString() + "'")
                        .SetCode(ErrCannotApplyOp)
                        .AddSpan(ue->GetStartLoc(), ue->GetEndLoc());
                }
            }
            break;
    }

    HIRNode *unNode = _builder.CreateUnary(rhsRes.HirNode, rhs.Type, tokenKindToHIRUk(ue->GetOp().Kind));
    
    if (rhs.IsUnknown() || !ok) {
        return { Value(Value::Unknown, ValueData(), rhs.Type, ue->GetStartLoc(), ue->GetEndLoc()), unNode };
    }

    double rhsVal;
    if (rhs.Type->IsInteger()) {
        rhsVal = std::get<0>(rhs.Data);
    }
    else {
        rhsVal = std::get<1>(rhs.Data);
    }
    double res = 0;
    switch (ue->GetOp().Kind) {
        #define CASE(n, op) case n: res = op rhsVal; break;
        CASE(TkMinus, -)
        CASE(TkBang, !)
        #undef CASE
    }
    switch (rhs.Type->GetKind()) {
        #define VAL(t) Value(Value::Const, ValueData(static_cast<t>(res)), rhs.Type, ue->GetStartLoc(), ue->GetEndLoc())
        case Type::Integer:
            return { VAL(int64_t), _builder.CreateLiteral(VAL(int64_t)) };
        case Type::Floating:
            return { VAL(double), _builder.CreateLiteral(VAL(double)) };
        #undef VAL
    }
}

Semantic::SemanticResult
Semantic::analyzeVE(VarExpr *ve) {
    auto varsCopy = _vars;
    while (!varsCopy.empty()) {
        auto &top = varsCopy.top();
        if (auto it = top.Vars.find(ve->GetName().Name); it != top.Vars.end()) {
            if (it->second.IsConst) {
                return { it->second.Val, _builder.CreateLiteral(it->second.Val) };
            }
            HIRNode *veNode = _builder.CreateLoadVar(it->second.Storage, it->second.Index);
            return { Value(Value::Unknown, ValueData(), it->second.Type, ve->GetStartLoc(), ve->GetEndLoc(), true), veNode };
        }
        varsCopy.pop();
    }
    if (auto it = _mod->Structs.find(ve->GetName().Name); it != _mod->Structs.end()) {
        return { Value(Value::TypeLit, ValueData(), new StructType(ve->GetName(), _mod, ve->GetStartLoc(), ve->GetEndLoc()), ve->GetStartLoc(), ve->GetEndLoc()),
                 nullptr };
    }
    if (auto it = _mod->Submods.find(ve->GetName().Name); it != _mod->Submods.end()) {
        return { Value(Value::Unknown, ValueData(), new ModuleType(it->second, ve->GetName().Start, ve->GetName().End), ve->GetName().Start, ve->GetName().End),
                 nullptr };
    }
    if (auto it = _mod->Imports.find(ve->GetName().Name); it != _mod->Imports.end()) {
        return { Value(Value::Unknown, ValueData(), new ModuleType(it->second, ve->GetName().Start, ve->GetName().End), ve->GetName().Start, ve->GetName().End),
                 nullptr };
    }
    _diag.Report(Error, "variable is undeclared in this scope")
        .SetCode(ErrUndeclaredVar)
        .AddSpan(ve->GetStartLoc(), ve->GetEndLoc(), "undeclared");
    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

Semantic::SemanticResult
Semantic::analyzeFCE(FuncCallExpr *fce) {
    FuncOverload *candidates = findFuncCandidates(fce->GetName().Name);
    if (!candidates) {
        _diag.Report(Error, "function is undeclared in this scope")
            .SetCode(ErrUndeclaredFunc)
            .AddSpan(fce->GetStartLoc(), fce->GetEndLoc(), "undeclared");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }

    for (auto &cand : candidates->Candidates) {
        resolveFuncSignature(&cand);
    }

    std::vector<Type *> argTypes;
    std::vector<SemanticResult> argResults;
    
    for (auto &a : fce->GetArgs()) {
        auto argRes = analyzeExpr(a);
        argResults.push_back(argRes);
        argTypes.push_back(argRes.Val.Type);
    }

    Function *bestFunc = resolveBestOverload(candidates, argTypes, fce->GetStartLoc(), fce->GetEndLoc());
    if (!bestFunc) {
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }

    std::vector<HIRNode *> hirArgs;
    for (int i = 0; i < argResults.size(); ++i) {
        auto res = implicitlyCast(argResults[i], &bestFunc->Args[i].Type);
        hirArgs.push_back(res.HirNode);
    }

    return { Value(Value::Unknown, ValueData(), bestFunc->RetType, fce->GetStartLoc(), fce->GetEndLoc()),
             _builder.CreateCall(bestFunc->GetMangledName(), hirArgs) };
}

Semantic::SemanticResult
Semantic::analyzeFE(FieldExpr *fe) {
    auto baseRes = analyzeExpr(fe->GetBase());

    while (baseRes.Val.Type->IsPointer()) {
        baseRes = ensureSafePointer(baseRes);
        Type *ptrBaseType = baseRes.Val.Type->AsPointer()->GetBaseType();
        baseRes.HirNode = _builder.CreateDereference(baseRes.HirNode, ptrBaseType);
        baseRes.Val.Type = ptrBaseType;
        baseRes.Val.IsLValue = true;
    }
    
    if (baseRes.Val.Type->IsModulePtr()) {
        Module *mod = baseRes.Val.Type->AsModulePtr()->GetMod();
        if (auto it = mod->Vars.find(fe->GetName().Name); it != mod->Vars.end()) {
            if (it->second.Access != Pub) {
                _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(fe->GetName().Start, fe->GetName().End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make variable '" + fe->GetName().Name + "' accessible")
                    .AddHelp("consider using a public method or API instead");
                return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
            }
            HIRNode *veNode = _builder.CreateLoadVar(it->second.Storage, it->second.Index);
            return { Value(Value::Unknown, ValueData(), it->second.Type, fe->GetStartLoc(), fe->GetEndLoc()), veNode };
        }
        if (auto it = mod->Structs.find(fe->GetName().Name); it != mod->Structs.end()) {
            return { Value(Value::TypeLit, ValueData(), new StructType(fe->GetName(), mod, fe->GetStartLoc(), fe->GetEndLoc()), fe->GetStartLoc(), fe->GetEndLoc()),
                    nullptr };
        }
        if (auto it = mod->Submods.find(fe->GetName().Name); it != mod->Submods.end()) {
            if (it->second->Access != Pub) {
                _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(fe->GetName().Start, fe->GetName().End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make module '" + fe->GetName().Name + "' accessible");
                return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
            }
            return { Value(Value::Unknown, ValueData(), new ModuleType(it->second, fe->GetName().Start, fe->GetName().End),
                     fe->GetName().Start, fe->GetName().End), nullptr };
        }
        _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is undeclared in module '" + mod->Name + "'")
            .SetCode(ErrUndeclaredSymbol)
            .AddSpan(fe->GetName().Start, fe->GetName().End, "undeclared");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    else if (baseRes.Val.Type->IsStruct()) {
        bool baseIsThis = baseRes.Val.Kind == Value::This;
        auto *st = baseRes.Val.Type->AsStruct();
        auto &s = st->GetBaseMod()->Structs.at(st->GetName().Name);
        auto it = std::find_if(s.Fields.begin(), s.Fields.end(), [&](const Field &f) {
            return f.Var.Name.Name == fe->GetName().Name;
        });
        if (it == s.Fields.end()) {
            _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is undeclared in struct '" + s.Name.Name + "'")
                .SetCode(ErrUndeclaredSymbol)
                .AddSpan(fe->GetName().Start, fe->GetName().End, "undeclared");
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }
        if (it->Access != Pub && !baseIsThis) {
            _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is private")
                .SetCode(ErrPrivateSymbol)
                .AddSpan(fe->GetName().Start, fe->GetName().End, "private symbol")
                .AddHelp("consider using the 'pub' keyword to make field '" + fe->GetName().Name + "' accessible")
                .AddHelp("consider using a public method or API instead");
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }
        if (it->IsStatic && baseRes.Val.Kind != Value::TypeLit) {
            _diag.Report(Error, "static field '" + fe->GetName().Name + "' cannot be accessed via an instance")
                .SetCode(ErrAccessStaticFromInstance)
                .AddSpan(fe->GetName().Start, fe->GetName().End, "static symbol")
                .AddHelp("consider using the type name instead: '" + baseRes.Val.Type->ToString() + '.' + fe->GetName().Name + "'");
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }
        else if (!it->IsStatic && baseRes.Val.Kind == Value::TypeLit) {
            _diag.Report(Error, "non-static field '" + fe->GetName().Name + "' cannot be accessed via a type")
                .SetCode(ErrAccessNonStaticFromType)
                .AddSpan(fe->GetName().Start, fe->GetName().End, "non-static symbol")
                .AddHelp("consider using an instance of '" + baseRes.Val.Type->ToString() + "' or making the field static");
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        HIRNode *hirNode = nullptr;
        if (!it->IsStatic) {
            int index = 0;
            for (auto &f : s.Fields) {
                if (f == *it) {
                    break;
                }
                if (!f.IsStatic) {
                    ++index;
                }
            }
            if (baseIsThis) {
                baseRes.HirNode = _builder.CreateDereference(baseRes.HirNode, baseRes.Val.Type);
            }
            hirNode = _builder.CreateFieldExpr(baseRes.HirNode, baseRes.Val.Type, index);
        }
        else {
            hirNode = _builder.CreateLoadVar(Static, _staticFields[s.GetMangledName() + "." + it->Var.Name.Name]);
        }
        return { Value(Value::Unknown, ValueData(), it->Var.Type, fe->GetStartLoc(), fe->GetEndLoc(), true),
                 hirNode };
    }
    else if (auto *slice = baseRes.Val.Type->AsSlice()) {
        if (fe->GetName().Name == "Data") {
            HIRNode *hirNode = _builder.CreateFieldExpr(baseRes.HirNode, baseRes.Val.Type, 0);
            return { Value(Value::Unknown, ValueData(), new PointerType(slice->GetBaseType(), slice->GetStartLoc(), slice->GetEndLoc()), fe->GetStartLoc(), fe->GetEndLoc(), true),
                     hirNode };
        }
        else if (fe->GetName().Name == "Length") {
            HIRNode *hirNode = _builder.CreateFieldExpr(baseRes.HirNode, baseRes.Val.Type, 1);
            return { Value(Value::Unknown, ValueData(), new SizeType(true, slice->GetStartLoc(), slice->GetEndLoc()), fe->GetStartLoc(), fe->GetEndLoc(), true),
                     hirNode };
        }
    }
    _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is undeclared")
        .SetCode(ErrUndeclaredSymbol)
        .AddSpan(fe->GetName().Start, fe->GetName().End, "undeclared");
    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

Semantic::SemanticResult
Semantic::analyzeMCE(MethodCallExpr *mce) {
    auto baseRes = analyzeExpr(mce->GetBase());

    while (baseRes.Val.Type->IsPointer()) {
        baseRes = ensureSafePointer(baseRes);
        Type *ptrBaseType = baseRes.Val.Type->AsPointer()->GetBaseType();
        baseRes.HirNode = _builder.CreateDereference(baseRes.HirNode, ptrBaseType);
        baseRes.Val.Type = ptrBaseType;
        baseRes.Val.IsLValue = true;
    }
    
    if (baseRes.Val.Type->IsModulePtr()) {
        Module *mod = baseRes.Val.Type->AsModulePtr()->GetMod();
        if (auto it = mod->FuncOverloads.find(mce->GetName().Name); it != mod->FuncOverloads.end()) {
            FuncOverload *candidates = &it->second;
            
            std::vector<Type *> argTypes;
            std::vector<SemanticResult> argResults;
            
            for (auto &a : mce->GetArgs()) {
                auto argRes = analyzeExpr(a);
                argResults.push_back(argRes);
                argTypes.push_back(argRes.Val.Type);
            }

            Function *bestFunc = resolveBestOverload(candidates, argTypes, mce->GetStartLoc(), mce->GetEndLoc());
            if (!bestFunc) {
                return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
            }
            if (bestFunc->Access != Pub) {
                _diag.Report(Error, "symbol '" + mce->GetName().Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(mce->GetName().Start, mce->GetName().End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make function '" + mce->GetName().Name + "' accessible");
                return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
            }

            std::vector<HIRNode *> hirArgs;
            for (int i = 0; i < argResults.size(); ++i) {
                auto res = implicitlyCast(argResults[i], &bestFunc->Args[i].Type);
                hirArgs.push_back(res.HirNode);
            }

            return { Value(Value::Unknown, ValueData(), bestFunc->RetType, mce->GetStartLoc(), mce->GetEndLoc()),
                     _builder.CreateCall(bestFunc->GetMangledName(), hirArgs, mod) };
        }
        _diag.Report(Error, "symbol '" + mce->GetName().Name + "' is undeclared in module '" + mod->Name + "'")
            .SetCode(ErrUndeclaredSymbol)
            .AddSpan(mce->GetName().Start, mce->GetName().End, "undeclared");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    else if (baseRes.Val.Type->IsStruct()) {
        bool baseIsThis = baseRes.Val.Kind == Value::This;
        auto *st = baseRes.Val.Type->AsStruct();
        auto &s = st->GetBaseMod()->Structs.at(st->GetName().Name);
        std::string methodName = mce->GetName().Name;

        MethodOverload *candidates = nullptr;
        for (auto &o : s.Methods) {
            if (!o.Candidates.empty() && o.Candidates[0].Func.Name.Name == methodName) {
                candidates = &o;
                break;
            }
        }

        if (!candidates) {
            _diag.Report(Error, "struct '" + s.Name.Name + "' has no method named '" + methodName + "'")
                .SetCode(ErrUndeclaredSymbol)
                .AddSpan(mce->GetName().Start, mce->GetName().End);
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        std::vector<Type *> argTypes;
        std::vector<SemanticResult> argResults;
        
        for (auto &a : mce->GetArgs()) {
            auto argRes = analyzeExpr(a);
            argResults.push_back(argRes);
            argTypes.push_back(argRes.Val.Type);
        }

        std::vector<std::pair<Method *, int>> viableCandidates;
        for (auto &cand : candidates->Candidates) {
            if (cand.Func.Args.size() != argTypes.size()) {
                continue;
            }

            bool viable = true;
            int costSum = 0;
            for (int i = 0; i < argTypes.size(); ++i) {
                CastCost cost = checkCastCost(argTypes[i], cand.Func.Args[i].Type);
                if (cost == Incompatible) {
                    viable = false;
                    break;
                }
                costSum += cost;
            }

            if (viable) {
                viableCandidates.push_back({ &cand, costSum });
            }
        }

        if (viableCandidates.empty()) {
            _diag.Report(Error, "no matching method for call")
                .SetCode(ErrNoMatchingFunction)
                .AddSpan(mce->GetStartLoc(), mce->GetEndLoc());
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        Method *bestMethod = viableCandidates[0].first;
        int minCost = viableCandidates[0].second;
        bool isAmbiguous = false;

        for (int i = 1; i < viableCandidates.size(); ++i) {
            if (viableCandidates[i].second < minCost) {
                minCost = viableCandidates[i].second;
                bestMethod = viableCandidates[i].first;
                isAmbiguous = false;
            }
            else if (viableCandidates[i].second == minCost) {
                isAmbiguous = true;
            }
        }

        if (isAmbiguous) {
            _diag.Report(Error, "method call is ambiguous")
                .SetCode(ErrAmbiguousCall)
                .AddSpan(mce->GetStartLoc(), mce->GetEndLoc());
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        if (bestMethod->Access != Pub && !baseIsThis) {
            _diag.Report(Error, "symbol '" + methodName + "' is private")
                .SetCode(ErrPrivateSymbol)
                .AddSpan(mce->GetName().Start, mce->GetName().End, "private symbol")
                .AddHelp("consider using the 'pub' keyword to make method '" + methodName + "' accessible");
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        std::vector<HIRNode *> hirArgs;
        if (!bestMethod->IsStatic) {
            if (baseIsThis) {
                hirArgs.push_back(baseRes.HirNode);
            }
            else {
                hirArgs.push_back(_builder.CreateReference(baseRes.HirNode));
            }
        }
        std::string mangledName = s.GetMangledName() + "." + bestMethod->Func.Name.Name;
        for (int i = 0; i < argResults.size(); ++i) {
            auto res = implicitlyCast(argResults[i], &bestMethod->Func.Args[i].Type);
            hirArgs.push_back(res.HirNode);
            mangledName += res.Val.Type->ToString();
        }

        return { Value(Value::Unknown, ValueData(), bestMethod->Func.RetType, mce->GetStartLoc(), mce->GetEndLoc()),
                 _builder.CreateCall(mangledName, hirArgs) };
    }
    else {
        bool baseIsThis = baseRes.Val.Kind == Value::This;
        std::string methodName = mce->GetName().Name;

        MethodOverload *candidates = findPrimitiveMethodCandidates(baseRes.Val.Type, methodName);

        if (!candidates) {
            _diag.Report(Error, "type '" + baseRes.Val.Type->ToString() + "' has no method named '" + methodName + "'")
                .SetCode(ErrUndeclaredSymbol)
                .AddSpan(mce->GetName().Start, mce->GetName().End);
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        std::vector<Type *> argTypes;
        std::vector<SemanticResult> argResults;
        
        for (auto &a : mce->GetArgs()) {
            auto argRes = analyzeExpr(a);
            argResults.push_back(argRes);
            argTypes.push_back(argRes.Val.Type);
        }

        std::vector<std::pair<Method *, int>> viableCandidates;
        for (auto &cand : candidates->Candidates) {
            if (cand.Func.Args.size() != argTypes.size()) {
                continue;
            }

            bool viable = true;
            int costSum = 0;
            for (int i = 0; i < argTypes.size(); ++i) {
                CastCost cost = checkCastCost(argTypes[i], cand.Func.Args[i].Type);
                if (cost == Incompatible) {
                    viable = false;
                    break;
                }
                costSum += cost;
            }

            if (viable) {
                viableCandidates.push_back({ &cand, costSum });
            }
        }

        if (viableCandidates.empty()) {
            _diag.Report(Error, "no matching method for call")
                .SetCode(ErrNoMatchingFunction)
                .AddSpan(mce->GetStartLoc(), mce->GetEndLoc());
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        Method *bestMethod = viableCandidates[0].first;
        int minCost = viableCandidates[0].second;
        bool isAmbiguous = false;

        for (int i = 1; i < viableCandidates.size(); ++i) {
            if (viableCandidates[i].second < minCost) {
                minCost = viableCandidates[i].second;
                bestMethod = viableCandidates[i].first;
                isAmbiguous = false;
            }
            else if (viableCandidates[i].second == minCost) {
                isAmbiguous = true;
            }
        }

        if (isAmbiguous) {
            _diag.Report(Error, "method call is ambiguous")
                .SetCode(ErrAmbiguousCall)
                .AddSpan(mce->GetStartLoc(), mce->GetEndLoc());
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }

        std::vector<HIRNode *> hirArgs;
        if (!bestMethod->IsStatic) {
            if (baseIsThis) {
                hirArgs.push_back(baseRes.HirNode);
            }
            else {
                if (baseRes.Val.IsLValue) {
                    hirArgs.push_back(_builder.CreateReference(baseRes.HirNode)); 
                } 
                else {
                    HIRNode *tempAlloc = _builder.GetContext().CreateNode<HIRVarDeclStmt>("tmp.rvalue", baseRes.Val.Type, baseRes.HirNode, false, Stack);
                    hirArgs.push_back(tempAlloc);
                }
            }
        }
        
        std::string mangledName = bestMethod->Func.Parent->ToString() + "." + baseRes.Val.Type->ToString() + "." + bestMethod->Func.Name.Name;
        for (int i = 0; i < argResults.size(); ++i) {
            auto res = implicitlyCast(argResults[i], &bestMethod->Func.Args[i].Type);
            hirArgs.push_back(res.HirNode);
            mangledName += res.Val.Type->ToString();
        }

        return { Value(Value::Unknown, ValueData(), bestMethod->Func.RetType, mce->GetStartLoc(), mce->GetEndLoc()),
                 _builder.CreateCall(mangledName, hirArgs) };
    }
    _diag.Report(Error, "symbol '" + mce->GetName().Name + "' is undeclared")
        .SetCode(ErrUndeclaredSymbol)
        .AddSpan(mce->GetName().Start, mce->GetName().End, "undeclared");
    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

Semantic::SemanticResult
Semantic::analyzeSIE(StructInstanceExpr *sie) {
    const auto &path = sie->GetPath();
    auto *s = findStructByPath(path);
    std::string fullPath = path[0].Name;
    for (int i = 1; i < path.size(); ++i) {
        fullPath += '.' + path[i].Name;
    }

    if (!s) {
        _diag.Report(Error, "symbol '" + fullPath + "' is undeclared")
            .SetCode(ErrUndeclaredSymbol)
            .AddSpan(sie->GetPath().front().Start, sie->GetPath().back().End, "undeclared");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }

    bool allowInitPrivateFields = false;
    if (analyzingMethodOfStruct && *analyzingMethodOfStruct == *s) {
        allowInitPrivateFields = true;
    }
    
    std::vector<std::pair<int, HIRNode *>> fields;
    int index = 0;
    for (int i = 0; i < sie->GetFields().size(); ++i) {
        auto it = std::find_if(s->Fields.begin(), s->Fields.end(), [&](const Field &f) {
            return f.Var.Name.Name == sie->GetFields()[i].first.Name;
        });

        if (it == s->Fields.end()) {
            _diag.Report(Error, "symbol '" + sie->GetFields()[i].first.Name + "' is undeclared in structure '" + fullPath + "'")
                .SetCode(ErrUndeclaredSymbol)
                .AddSpan(sie->GetFields()[i].first.Start, sie->GetFields()[i].first.End);
            continue;
        }
        
        if (it->Access == Priv && !allowInitPrivateFields) {
            _diag.Report(Error, "struct '" + fullPath + "' has inaccessible fields and cannot be initialized directly from an external context")
                .SetCode(ErrPrivateSymbol)
                .AddSpan(sie->GetPath().front().Start, sie->GetPath().back().End);
            return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
        }
        if (it->IsStatic) {
            _diag.Report(Error, "cannot initialize static field '" + sie->GetFields()[i].first.Name + "' in a struct initializer")
                .SetCode(ErrInitStaticFieldInInitializer)
                .AddSpan(sie->GetFields()[i].first.Start, sie->GetFields()[i].first.End);
            continue;
        }

        auto res = analyzeExpr(sie->GetFields()[i].second);
        res = implicitlyCast(res, &it->Var.Type);
        fields.push_back({ index, res.HirNode });
        if (!it->IsStatic) {
            ++index;
        }
    }
    auto val = Value(Value::Const, ValueData(), new StructType(s->Name, s->Parent, sie->GetPath().front().Start, sie->GetPath().back().End),
                            sie->GetStartLoc(), sie->GetEndLoc());
    auto hitNode = _builder.CreateStructInstance(s->GetMangledName(), fields);
    return { val, hitNode };
}

Semantic::SemanticResult
Semantic::analyzeTE(TypeExpr *te) {
    return { Value(Value::TypeLit, ValueData(), te->GetType(), te->GetType()->GetStartLoc(), te->GetType()->GetEndLoc()),
             nullptr };
}

Semantic::SemanticResult
Semantic::analyzeNE(NilExpr *ne) {
    return { Value(Value::Nil, ValueData(), nullptr, ne->GetStartLoc(), ne->GetEndLoc()),
             nullptr };
}

Semantic::SemanticResult
Semantic::analyzeRE(RefExpr *re) {
    auto base = analyzeExpr(re->GetBase());
    if (!base.Val.IsLValue) {
        _diag.Report(Error, "cannot take reference of rvalue")
            .SetCode(ErrCannotTakeRef)
            .AddSpan(re->GetStartLoc(), re->GetEndLoc())
            .AddHelp("consider storing the value in a variable first: 'var tmp = " + std::string(re->GetBase()->GetStartLoc().getPointer(), re->GetBase()->GetEndLoc().getPointer() - re->GetStartLoc().getPointer()) + "' and getting reference via '&tmp'");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    base.Val.Type = new PointerType(base.Val.Type, base.Val.Type->GetStartLoc(), base.Val.Type->GetEndLoc());
    base.Val.Kind = Value::Unknown;
    return { base.Val, _builder.CreateReference(base.HirNode) };
}

Semantic::SemanticResult
Semantic::analyzeDE(DerefExpr *de) {
    auto base = analyzeExpr(de->GetBase());
    Type *baseType = base.Val.Type;
    if (!baseType->IsPointer()) {
        _diag.Report(Error, "type '" + baseType->ToString() + "' cannot be dereferenced")
            .SetCode(ErrCannotBeDeref)
            .AddSpan(de->GetStartLoc(), de->GetEndLoc())
            .AddHelp("found '" + baseType->ToString() + "' instead of a pointer; consider removing the dereference operator");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    base = ensureSafePointer(base);
    PointerType *ptr = baseType->AsPointer();
    base.Val.Type = ptr->GetBaseType();
    base.Val.Kind = Value::Unknown;
    return { base.Val, _builder.CreateDereference(base.HirNode, base.Val.Type) };
}

Semantic::SemanticResult
Semantic::analyzeNew(NewExpr *ne) {
    resolveType(&ne->GetType());
    Type *type = new PointerType(ne->GetType(), ne->GetType()->GetStartLoc(), ne->GetType()->GetEndLoc());
    HIRNode *res = nullptr;
    if (ne->GetExpr()) {
        auto expr = analyzeExpr(ne->GetExpr());
        if (ne->GetType()->IsSlice() && expr.Val.Type->IsArray()) {
            res = expr.HirNode;
        }
        else {
            expr = implicitlyCast(expr, &ne->GetType());
            res = expr.HirNode;
        }
    }
    return { Value(Value::Unknown, ValueData(), type, ne->GetStartLoc(), ne->GetEndLoc()), _builder.CreateNew(ne->GetType(), res) };
}

Semantic::SemanticResult
Semantic::analyzeAIE(ArrayInstanceExpr *aie) {
    if (aie->GetExprs().size() == 0) {
        // TODO: create error
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    auto fisrtExpr = analyzeExpr(aie->GetExprs()[0]);
    std::vector<HIRNode *> exprs = { fisrtExpr.HirNode };
    Type *arrType = fisrtExpr.Val.Type;
    for (int i = 1; i < aie->GetExprs().size(); ++i) {
        auto res = analyzeExpr(aie->GetExprs()[i]);
        res = implicitlyCast(res, &arrType);
        exprs.push_back(res.HirNode);
    }
    auto val = Value(Value::Unknown, ValueData(), new ArrayType(arrType,
                                                                new LiteralExpr(Value(Value::Const,
                                                                                ValueData(static_cast<int64_t>(aie->GetExprs().size())),
                                                                                new SizeType(true, aie->GetStartLoc(), aie->GetEndLoc()),
                                                                aie->GetStartLoc(), aie->GetEndLoc())), aie->GetStartLoc(), aie->GetEndLoc()),
                     aie->GetStartLoc(), aie->GetEndLoc());
    val.Type->AsArray()->SetSize(aie->GetExprs().size());
    auto hirNode = _builder.CreateArray(arrType, exprs);
    return { val, hirNode };
}

Semantic::SemanticResult
Semantic::analyzeAAE(ArrayAccessExpr *aae) {
    auto baseRes = analyzeExpr(aae->GetBase());
    auto indexRes = analyzeExpr(aae->GetIndex());

    if (!indexRes.Val.Type->IsSizeType()) {
        _diag.Report(Error, "array index must be an usize")
            .SetCode(ErrCannotImplCast)
            .AddSpan(aae->GetIndex()->GetStartLoc(), aae->GetIndex()->GetEndLoc());
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }

    Type *resultType = nullptr;
    HIRNode *hirBase = baseRes.HirNode;

    if (baseRes.Val.Type->IsArray()) {
        resultType = baseRes.Val.Type->AsArray()->GetBaseType();
    }
    else if (baseRes.Val.Type->IsSlice()) {
        resultType = baseRes.Val.Type->AsSlice()->GetBaseType();
        hirBase = _builder.CreateFieldExpr(hirBase, baseRes.Val.Type, 0);
    }
    else {
        _diag.Report(Error, "type '" + baseRes.Val.Type->ToString() + "' does not support indexing")
            .SetCode(ErrCannotApplyOp)
            .AddSpan(aae->GetBase()->GetStartLoc(), aae->GetBase()->GetEndLoc());
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }

    return { Value(Value::Unknown, ValueData(), resultType, aae->GetStartLoc(), aae->GetEndLoc(), true),
             _builder.CreateArrayAccess(hirBase, baseRes.Val.Type, indexRes.HirNode) };
}

void
Semantic::createVar(std::string name, Variable var) {
    var.Index = var.Index == -1 ? _currentFuncVarCount++ : var.Index;
    _vars.top().Vars.emplace(name, var);
}

Type *
Semantic::resolveType(Type **t) {
    if (!t) {
        return nullptr;
    }
    if (!*t) {
        return nullptr;
    }
    
    switch ((*t)->GetKind()) {
        case Type::Tuple: {
            TupleType *tuple = (*t)->AsTuple();
            std::vector<Type *> newTypes;
            for (auto &t : tuple->GetTypes()) {
                newTypes.push_back(resolveType(&t));
            }
            tuple->SetTypes(newTypes);
            return *t;
        }
        case Type::Pointer: {
            Type *base = (*t)->AsPointer()->GetBaseType();
            (*t)->AsPointer()->SetBaseType(resolveType(&base));
            return *t;
        }
        case Type::Slice: {
            Type *base = (*t)->AsSlice()->GetBaseType();
            (*t)->AsSlice()->SetBaseType(resolveType(&base));
            return *t;
        }
        case Type::Array: {
            Type *base = (*t)->AsArray()->GetBaseType();
            (*t)->AsArray()->SetBaseType(resolveType(&base));
            return *t;
        }
    }

    if (!(*t)->Is(Type::Unknown)) {
        return *t;
    }

    UnknownNamedType *unt = (*t)->AsUnknownNamedType();
    const auto &path = unt->GetPath();

    if (auto *st = findStructByPath(path)) {
        delete *t;
        *t = new StructType(path.back(), st->Parent, unt->GetStartLoc(), unt->GetEndLoc());
        return *t;
    }

    /* TODO: implement findTraitByPath
    if (auto *trt = findTraitByPath(path)) {
        delete *t;
        *t = new TraitType(path.back(), trt->Parent, unt->GetStartLoc(), unt->GetEndLoc());
        return *t;
    }
    */

    _diag.Report(Error, "unknown type '" + unt->ToString() + "' at this scope")
        .SetCode(ErrUnknownType)
        .AddSpan(path.front().Start, path.back().End);
    return *t;
}

Type *
Semantic::getCommonType(Type *lhs, Type *rhs) {
    if (*lhs == *rhs) {
        return lhs;
    }
    if (lhs->IsInteger() && rhs->IsInteger()) {
        auto *lhsT = lhs->AsInteger();
        auto *rhsT = rhs->AsInteger();
        if (lhsT->IsUnsigned() && rhsT->IsUnsigned() || !lhsT->IsUnsigned() && !rhsT->IsUnsigned()) {
            if (lhsT->GetBitWidth() > rhsT->GetBitWidth()) {
                return lhs;
            }
            else {
                return rhs;
            }
        }
        if (lhsT->IsUnsigned()) {
            if (lhsT->GetBitWidth() >= rhsT->GetBitWidth()) {
                return lhs;
            }
            else {
                return rhs;
            }
        }
        if (rhsT->IsUnsigned()) {
            if (rhsT->GetBitWidth() >= lhsT->GetBitWidth()) {
                return rhs;
            }
            else {
                return lhs;
            }
        }
    }
    if (lhs->IsFloating() && rhs->IsFloating()) {
        auto *lhsT = lhs->AsFloating();
        auto *rhsT = rhs->AsFloating();
        if (lhsT->IsDouble()) {
            return lhs;
        }
        else {
            return rhs;
        }
    }
    if (lhs->IsFloating() && rhs->IsInteger()) {
        auto *lhsT = lhs->AsFloating();
        auto *rhsT = rhs->AsInteger();
        if (lhsT->IsFloat() && rhsT->GetBitWidth() >= 32) {
            _diag.Report(Warning, "casting types can lead to loss of precision")
                .SetCode(WarnLostPrecision)
                .AddSpan(rhs->GetStartLoc(), rhs->GetEndLoc(), "can lost of precision");
        }
        else if (lhsT->IsDouble() && rhsT->GetBitWidth() >= 64) {
            _diag.Report(Warning, "casting types can lead to loss of precision")
                .SetCode(WarnLostPrecision)
                .AddSpan(rhs->GetStartLoc(), rhs->GetEndLoc(), "can lost of precision");
        }
        return lhs;
    }
    if (lhs->IsInteger() && rhs->IsFloating()) {
        auto *lhsT = lhs->AsInteger();
        auto *rhsT = rhs->AsFloating();
        if (rhsT->IsFloat() && lhsT->GetBitWidth() >= 32) {
            _diag.Report(Warning, "casting types can lead to loss of precision")
                .SetCode(WarnLostPrecision)
                .AddSpan(lhs->GetStartLoc(), lhs->GetEndLoc(), "can lost of precision");
        }
        else if (rhsT->IsDouble() && lhsT->GetBitWidth() >= 64) {
            _diag.Report(Warning, "casting types can lead to loss of precision")
                .SetCode(WarnLostPrecision)
                .AddSpan(lhs->GetStartLoc(), lhs->GetEndLoc(), "can lost of precision");
        }
        return rhs;
    }
    _diag.Report(Error, "cannot find common type")
        .SetCode(ErrCannotFindCommonType)
        .AddSpan(lhs->GetStartLoc(), rhs->GetEndLoc())
        .AddHelp("сonsider using an explicit cast");
    return lhs;
}


Type *
Semantic::getCommonTypeForOp(Type *lhs, Type *rhs, const Token op, llvm::SMLoc s, llvm::SMLoc e) {
    switch (op.Kind) {
        case TkPlus:
        case TkMinus:
        case TkStar:
        case TkSlash:
        case TkPercent:
        case TkLt:
        case TkGt:
        case TkLtEq:
        case TkGtEq:
            if (lhs->IsNumber() && rhs->IsNumber()) {
                return getCommonType(lhs, rhs);
            }
            break;
        case TkEqEq:
        case TkNotEq:
            return getCommonType(lhs, rhs);
        case TkLogAnd:
        case TkLogOr:
            if (lhs->IsInteger() && rhs->IsInteger()) {
                auto *lhsT = lhs->AsInteger();
                auto *rhsT = rhs->AsInteger();
                if (lhsT->GetBitWidth() == rhsT->GetBitWidth() && lhsT->GetBitWidth() == 1) {
                    return lhs;
                }
            }
            break;
    }
    _diag.Report(Error, "cannot apply operator '" + op.Val + "' to types '" + lhs->ToString() + "' and '" + rhs->ToString() + "'")
        .SetCode(ErrCannotApplyOp)
        .AddSpan(s == llvm::SMLoc() ? lhs->GetStartLoc() : s, e == llvm::SMLoc() ? rhs->GetEndLoc() : e);
    return lhs;
}

Semantic::SemanticResult
Semantic::implicitlyCast(SemanticResult res, Type **expectedType) {
    resolveType(&res.Val.Type);
    resolveType(expectedType);

    if (res.Val.Kind == Value::Nil) {
        if ((*expectedType)->IsPointer()) {
            res.Val.Type = *expectedType;
            return { res.Val, _builder.CreateNil() };
        }
        _diag.Report(Error, "cannot implicitly cast 'nil' to '" + (*expectedType)->ToString() + "'")
            .SetCode(ErrCannotImplCast)
            .AddSpan(res.Val.Start, res.Val.End);

        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    
    Type *src = res.Val.Type;
    Type *dst = *expectedType;

    if (*src == *dst) {
        return res;
    }

    if (src->IsArray() && dst->IsSlice()) {
        auto *arr = src->AsArray();
        auto *slice = dst->AsSlice();
        if (*arr->GetBaseType() == *slice->GetBaseType()) {
            HIRNode *arrayPtrNode = nullptr;
            if (res.Val.IsLValue) {
                arrayPtrNode = _builder.CreateReference(res.HirNode);
            }
            else {
                arrayPtrNode = _builder.GetContext().CreateNode<HIRVarDeclStmt>("tmp.array", src, res.HirNode, false, Stack);
            }
            
            auto emtpyLoc = llvm::SMLoc();
            std::vector<std::pair<int, HIRNode *>> fields = {
                { 0, arrayPtrNode },
                { 1, _builder.CreateLiteral(Value(Value::Const, ValueData(arr->GetSize()), new SizeType(true, emtpyLoc, emtpyLoc), emtpyLoc, emtpyLoc)) }
            };
            std::vector<Type *> sliceFields = {
                new PointerType(slice->GetBaseType(), llvm::SMLoc(), llvm::SMLoc()),
                new SizeType(true, emtpyLoc, emtpyLoc)
            };
            _builder.CreateStruct("slice." + arr->GetBaseType()->ToString(), sliceFields);
            auto hirNode = _builder.CreateStructInstance("slice." + arr->GetBaseType()->ToString(), fields);
            res.Val.Type = dst;
            return { res.Val, hirNode };
        }
    }

    if (src->IsInteger() && dst->IsInteger()) {
        auto *srcI = src->AsInteger();
        auto *dstI = dst->AsInteger();

        if (dstI->GetBitWidth() >= srcI->GetBitWidth()) {
            if (srcI->IsUnsigned() == dstI->IsUnsigned() || dstI->GetBitWidth() > srcI->GetBitWidth()) {
                CastKind kind = srcI->IsUnsigned() ? ZeroExtend : SignExtend;
                res.Val.Type = dst;
                res.HirNode = _builder.CreateCast(kind, res.HirNode, src, dst);
                return res;
            }
        }
    }

    if (src->IsInteger() && dst->IsFloating()) {
        res.Val.Type = dst;
        if (res.Val.Kind == Value::Const) {
            double d = static_cast<double>(std::get<0>(res.Val.Data));
            res.Val.Data = ValueData(d);
        }
        res.HirNode = _builder.CreateCast(IntToFloat, res.HirNode, src, dst);
        return res;
    }

    if (src->IsFloating() && dst->IsFloating()) {
        auto *srcF = src->AsFloating();
        auto *dstF = dst->AsFloating();

        if (dstF->IsDouble() && srcF->IsFloat()) {
            res.Val.Type = dst;
            if (res.Val.Kind == Value::Const) {
                double d = std::get<1>(res.Val.Data); 
                res.Val.Data = ValueData(d);
            }
            res.HirNode = _builder.CreateCast(FPExtend, res.HirNode, src, dst); 
            return res;
        }
        else if (dstF->IsFloat() == srcF->IsFloat() || dstF->IsDouble() == srcF->IsDouble()) {
            return res;
        }
    }

    _diag.Report(Error, "cannot implicitly cast '" + src->ToString() + "' to '" + dst->ToString() + "'")
        .SetCode(ErrCannotImplCast)
        .AddSpan(res.Val.Start, res.Val.End);

    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

Semantic::CastCost
Semantic::checkCastCost(Type *src, Type *dst) {
    if (*src == *dst) {
        return Exact;
    }

    if (src->IsInteger() && dst->IsInteger()) {
        auto *srcI = src->AsInteger();
        auto *dstI = dst->AsInteger();

        if (dstI->GetBitWidth() >= srcI->GetBitWidth()) {
            if (srcI->IsUnsigned() == dstI->IsUnsigned() && dstI->GetBitWidth() == srcI->GetBitWidth()) {
                return Exact;
            }
            if (srcI->IsUnsigned() == dstI->IsUnsigned() || dstI->GetBitWidth() > srcI->GetBitWidth()) {
                return SafeImplicit;
            }
        }
    }

    if (src->IsInteger() && dst->IsFloating()) {
        return SafeImplicit;
    }

    if (src->IsFloating() && dst->IsFloating()) {
        auto *srcF = src->AsFloating();
        auto *dstF = dst->AsFloating();

        if (dstF->IsDouble() && srcF->IsFloat()) {
            return SafeImplicit;
        }
        else if (dstF->IsFloat() == srcF->IsFloat() || dstF->IsDouble() == srcF->IsDouble()) {
            return Exact;
        }
    }

    return Incompatible;
}

Function *
Semantic::resolveBestOverload(FuncOverload *candidates, const std::vector<Type *> &argTypes, llvm::SMLoc start, llvm::SMLoc end) {
    std::vector<std::pair<Function *, int>> viableCandidates;

    for (auto &cand : candidates->Candidates) {
        if (cand.Args.size() != argTypes.size()) {
            continue; 
        }

        bool viable = true;
        int costSum = 0;
        for (int i = 0; i < argTypes.size(); ++i) {
            CastCost cost = checkCastCost(argTypes[i], cand.Args[i].Type);
            if (cost == Incompatible) {
                viable = false;
                break;
            }
            costSum += cost;
        }

        if (viable) {
            viableCandidates.push_back({ &cand, costSum });
        }
    }

    if (viableCandidates.empty()) {
        _diag.Report(Error, "no matching function for call")
            .SetCode(ErrNoMatchingFunction)
            .AddSpan(start, end);
        return nullptr;
    }

    Function *bestCand = viableCandidates[0].first;
    int minCost = viableCandidates[0].second;
    bool isAmbiguous = false;

    for (int i = 1; i < viableCandidates.size(); ++i) {
        if (viableCandidates[i].second < minCost) {
            minCost = viableCandidates[i].second;
            bestCand = viableCandidates[i].first;
            isAmbiguous = false;
        }
        else if (viableCandidates[i].second == minCost) {
            isAmbiguous = true;
        }
    }

    if (isAmbiguous) {
        _diag.Report(Error, "call is ambiguous")
            .SetCode(ErrAmbiguousCall)
            .AddSpan(start, end);
        return nullptr;
    }

    return bestCand;
}

Semantic::SemanticResult
Semantic::ensureSafePointer(SemanticResult res) {
    if (res.Val.Kind == Value::Nil) {
        _diag.Report(Error, "attempt to dereference a nil value")
            .SetCode(ErrNilDeref)
            .AddSpan(res.Val.Start, res.Val.End);
        return res;
    }

    if (res.Val.Type->IsPointer() && res.Val.Kind == Value::Unknown) {
        auto mgr = _diag.GetSourceMgr();
        auto &buff = mgr->getBufferInfo(mgr->FindBufferContainingLoc(res.Val.Start));
        auto lineAndCol = mgr->getLineAndColumn(res.Val.Start);
        std::string pos = buff.Buffer->getBufferIdentifier().str() + ":" + std::to_string(lineAndCol.first) + ":" + std::to_string(lineAndCol.second);
        res.HirNode = _builder.CreateNilCheck(res.HirNode, pos);
    }

    return res;
}

HIRBinaryKind
Semantic::tokenKindToHIRBk(TokenKind kind) {
    switch (kind) {
        case TkPlus:
            return HIRBkAdd;
        case TkMinus:
            return HIRBkSub;
        case TkStar:
            return HIRBkMul;
        case TkSlash:
            return HIRBkDiv;
        case TkPercent:
            return HIRBkRem;
        case TkLt:
            return HIRBkLt;
        case TkGt:
            return HIRBkGt;
        case TkLtEq:
            return HIRBkLtEq;
        case TkGtEq:
            return HIRBkGtEq;
        case TkEqEq:
            return HIRBkEq;
        case TkNotEq:
            return HIRBkNEq;
        case TkLogAnd:
            return HIRBkAnd;
        case TkLogOr:
            return HIRBkOr;
    }
}

HIRUnaryKind
Semantic::tokenKindToHIRUk(TokenKind kind) {
    switch (kind) {
        case TkBang:
            return HIRUkNot;
        case TkMinus:
            return HIRUkMinus;
    }
}

}
