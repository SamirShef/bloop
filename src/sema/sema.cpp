#include <utils/types/types.h>
#include <sema/sema.h>
#include <cmath>

namespace bloop {

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
        NODE(NkFuncDeclStmt, analyzeFuncBody, FuncDeclStmt);
        NODE(NkUsingStmt, analyzeUS, UsingStmt);
        NODE(NkRetStmt, analyzeRS, RetStmt);
        NODE(NkIfElseStmt, analyzeIES, IfElseStmt);
        default: {
            _diag.Report(Error, "compiler limitation: statement type is currently unimplemented")
                .SetCode(ErrUnimplementedStmt)
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
        if (vds->GetExpr()) {
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
    if (auto it = top.VarsMap.find(vds->GetName().Name); it != top.VarsMap.end()) {
        _diag.Report(Error, "redefinition of '" + vds->GetName().Name + "'")
            .SetCode(ErrRedefinition)
            .AddSpan(top.Vars[it->second].Name.Start, top.Vars[it->second].Name.End, "previous definition is here")
            .AddSpan(vds->GetName().Start, vds->GetName().End, "redefinition here");
        return;
    }

    Variable var(vds->GetName(), vds->GetType(), vds->IsConst(), vds->GetAccess(), val, _vars.size() == 1 ? Static : Stack, top.Vars.size());
    if (_vars.size() == 1) {
        _mod->Vars.emplace(vds->GetName().Name, var);
    }
    std::string mangledName = _vars.size() == 1 ? _mod->ToString() + "." + vds->GetName().Name : vds->GetName().Name;
    _builder.CreateVar(mangledName, vds->GetType(), exprRes.HirNode, var.Storage, vds->IsConst());
    createVar(vds->GetName().Name, var);
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

    Function func(fds->GetName(), nullptr, fds->GetArgs(), fds->GetAccess(), Static, _mod);
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
        resolveType(&func->Args[i].Type);
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

    _funcsRetTypes.push(func->RetType);
    _vars.push({});

    for (int i = 0; i < func->Args.size(); ++i) {
        auto &a = func->Args[i];
        createVar(a.Name.Name, Variable(a.Name, a.Type, false, Priv, Value::GetIncorrectValue(), Parameter, i));
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
    func->RetType = fds->GetRetType();
    func->HirNode->GetRetType() = fds->GetRetType();

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
Semantic::analyzeUS(UsingStmt *us) {
    std::string modName = us->GetPath().Name;
    
    if (_mod->Imports.count(modName)) {
        return; 
    }

    auto it = _graph.find(modName);
    if (it == _graph.end()) {
        _diag.Report(Error, "module '" + modName + "' not found in project graph")
            .SetCode(ErrModNotFound)
            .AddSpan(us->GetStartLoc(), us->GetEndLoc());
        return;
    }

    const FileNode &node = it->second;

    if (!node.Mod) {
        _diag.Report(Error, "module '" + modName + "' is not loaded or has errors")
            .SetCode(ErrModNotLoaded)
            .AddSpan(us->GetStartLoc(), us->GetEndLoc());
        return;
    }

    _mod->Imports[modName] = node.Mod;

    for (auto &[name, obj] : node.Mod->Vars) {
        _builder.CreateVar(node.Mod->ToString() + "." + name, obj.Type, nullptr, Extern);
    }
    for (auto &[name, candidates] : node.Mod->FuncOverloads) {
        for (auto &c : candidates.Candidates) {
            std::vector<HIRFuncArgument> hirArgs;
            for (int i = 0; i < c.Args.size(); ++i) {
                auto &a = c.Args[i];
                hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
            }
            _builder.CreateFunc(c.GetMangledName(), c.RetType, hirArgs, name == "main", true);
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
    implicitlyCast(condRes, &boolType);
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

Semantic::SemanticResult
Semantic::analyzeExpr(Expr *expr) {
    #define NODE(k, f, t) case k: return f(static_cast<t *>(expr));
    switch (expr->GetKind()) {
        NODE(NkBinaryExpr, analyzeBE, BinaryExpr);
        NODE(NkLitExpr, analyzeLE, LiteralExpr);
        NODE(NkUnaryExpr, analyzeUE, UnaryExpr);
        NODE(NkVarExpr, analyzeVE, VarExpr);
        NODE(NkFuncCallExpr, analyzeFCE, FuncCallExpr);
        NODE(NkFieldExpr, analyzeFE, FieldExpr);
        NODE(NkMethodCallExpr, analyzeMCE, MethodCallExpr);
        default: {
            _diag.Report(Error, "compiler limitation: expression type is currently unimplemented")
                .SetCode(ErrUnimplementedExpr)
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

    HIRNode *binNode = _builder.CreateBinary(resultType, lhsRes.HirNode, rhsRes.HirNode, tokenKindToHIRBk(be->GetOp().Kind));

    if (lhs.IsUnknown() || rhs.IsUnknown()) {
        return { Value(Value::Unknown, ValueData(), resultType, be->GetStartLoc(), be->GetEndLoc()), binNode };
    }

    // TODO: add suporting of strings
    double lhsVal;
    double rhsVal;
    if (lhs.Type->IsInteger()) {
        lhsVal = std::get<0>(lhs.Data);
    }
    else {
        lhsVal = std::get<1>(lhs.Data);
    }
    if (rhs.Type->IsInteger()) {
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
            return { VAL(int64_t), _builder.CreateLiteral(VAL(int64_t)) };
        case Type::Floating:
            return { VAL(double), _builder.CreateLiteral(VAL(double)) };
        // TODO: add suporting of strings
        #undef VAL
    }
}

Semantic::SemanticResult
Semantic::analyzeLE(LiteralExpr *le) {
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

    HIRNode *unNode = _builder.CreateUnary(rhsRes.HirNode, tokenKindToHIRUk(ue->GetOp().Kind));
    
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
        if (auto it = top.VarsMap.find(ve->GetName().Name); it != top.VarsMap.end()) {
            if (top.Vars[it->second].IsConst) {
                return { top.Vars[it->second].Val, _builder.CreateLiteral(top.Vars[it->second].Val) };
            }
            HIRNode *veNode = _builder.CreateLoadVar(top.Vars[it->second].Storage, top.Vars[it->second].Index);
            return { Value(Value::Unknown, ValueData(), top.Vars[it->second].Type, ve->GetStartLoc(), ve->GetEndLoc()), veNode };
        }
        varsCopy.pop();
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
    if (baseRes.Val.Type->IsModulePtr()) {
        Module *mod = baseRes.Val.Type->AsModulePtr()->GetMod();
        if (auto it = mod->Vars.find(fe->GetName().Name); it != mod->Vars.end()) {
            if (it->second.Access != Pub) {
                _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is private")
                    .SetCode(ErrPrivateSymbol)
                    .AddSpan(fe->GetName().Start, fe->GetName().End, "private symbol")
                    .AddHelp("consider using the 'pub' keyword to make field '" + fe->GetName().Name + "' accessible")
                    .AddHelp("consider using a public method or API instead");
                return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
            }
            HIRNode *veNode = _builder.CreateLoadVar(it->second.Storage, it->second.Index, mod);
            return { Value(Value::Unknown, ValueData(), it->second.Type, fe->GetStartLoc(), fe->GetEndLoc()), veNode };
        }
        _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is undeclared in module '" + mod->Name + "'")
            .SetCode(ErrUndeclaredSymbol)
            .AddSpan(fe->GetName().Start, fe->GetName().End, "undeclared");
        return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
    }
    _diag.Report(Error, "symbol '" + fe->GetName().Name + "' is undeclared")
        .SetCode(ErrUndeclaredSymbol)
        .AddSpan(fe->GetName().Start, fe->GetName().End, "undeclared");
    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

Semantic::SemanticResult
Semantic::analyzeMCE(MethodCallExpr *mce) {
    auto baseRes = analyzeExpr(mce->GetBase());
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
    _diag.Report(Error, "symbol '" + mce->GetName().Name + "' is undeclared")
        .SetCode(ErrUndeclaredSymbol)
        .AddSpan(mce->GetName().Start, mce->GetName().End, "undeclared");
    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
}

void
Semantic::createVar(std::string name, Variable var) {
    var.Index = var.Index == -1 ? _vars.top().Vars.size() : var.Index;
    _vars.top().Vars.push_back(var);
    _vars.top().VarsMap.emplace(name, var.Index);
}

Type *
Semantic::resolveType(Type **t) {
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
    if (auto *st = findStruct(unt->GetName().Name)) {
        delete *t;
        *t = new StructType(unt->GetName(), _mod, unt->GetStartLoc(), unt->GetEndLoc());
        return *t;
    }
    if (auto *tr = findTrait(unt->GetName().Name)) {
        delete *t;
        *t = new TraitType(unt->GetName(), _mod, unt->GetStartLoc(), unt->GetEndLoc());
        return *t;
    }
    _diag.Report(Error, "unknown type `" + unt->GetName().Name + "` at this scope")
        .SetCode(ErrUnknownType)
        .AddSpan(unt->GetName().Start, unt->GetName().End);
    return *t;
}

Type *
Semantic::getCommonType(Type *lhs, Type *rhs) {
    if (lhs == rhs) {
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
            // TODO: add suporting of strings
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
    
    Type *src = res.Val.Type;
    Type *dst = *expectedType;

    if (*src == *dst) {
        return res;
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
    if (src == dst) {
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