#include <utils/types/types.h>
#include <sema/sema.h>
#include <cmath>

namespace bloop {

void
Semantic::analyzeStmt(Stmt *stmt) {
    if (!stmt) {
        return;
    }

    #define NODE(k, f, t) case k: return f(static_cast<t *>(stmt));
    switch (stmt->GetKind()) {
        NODE(NkVarDeclStmt, analyzeVDS, VarDeclStmt);
        NODE(NkFuncDeclStmt, analyzeFDS, FuncDeclStmt);
        NODE(NkUsingStmt, analyzeUS, UsingStmt);
        NODE(NkRetStmt, analyzeRS, RetStmt);
    }
    #undef NODE
}

void
Semantic::analyzeVDS(VarDeclStmt *vds) {
    auto exprRes = vds->GetExpr() ? analyzeExpr(vds->GetExpr()) : SemanticResult { Value(Value::Unknown, ValueData(), nullptr, llvm::SMLoc(), llvm::SMLoc()), nullptr };
    Value val = exprRes.Val;
    if (vds->GetType()) {
        resolveType(&vds->GetType());
        if (!vds->GetExpr()) {
            switch (vds->GetType()->GetKind()) {
                case Type::Integer:
                    val = Value(Value::Const, ValueData((int64_t)0), vds->GetType(), llvm::SMLoc(), llvm::SMLoc());
                    break;
                case Type::Floating:
                    val = Value(Value::Const, ValueData((double)0), vds->GetType(), llvm::SMLoc(), llvm::SMLoc());
                    break;
                default:
                    _diag.Report(Error, "cannot get default value for this type")
                        .SetCode(ErrCannotGetDefault)
                        .AddSpan(vds->GetType()->GetStartLoc(), vds->GetType()->GetEndLoc());
                    val = Value::GetIncorrectValue();
            }
        }
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

    Variable var(vds->GetName(), vds->GetType(), vds->IsConst(), vds->GetAccess(), val, _vars.size() == 1 ? Static : Stack);
    if (_vars.size() == 1) {
        _mod->Vars.emplace(vds->GetName().Name, var);
    }
    _builder.CreateVar(vds->GetName().Name, vds->GetType(), exprRes.HirNode, _vars.size() == 1 ? Static : Stack, vds->IsConst());
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
    Function func(fds->GetName(), fds->GetRetType(), fds->GetArgs(), fds->GetAccess(), Static);
    candidates->Candidates.push_back(func);
    std::vector<HIRFuncArgument> hirArgs;
    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        auto &a = fds->GetArgs()[i];
        hirArgs.push_back(HIRFuncArgument(a.Name.Name, a.Type, a.DefaultVal ? analyzeExpr(a.DefaultVal).HirNode : nullptr));
    }
    
    auto *funcHir = _builder.CreateFunc(fds->GetName().Name, fds->GetRetType(), hirArgs);

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
    funcHir->GetRetType() = fds->GetRetType();
    _funcsRetTypes.pop();

    if (!hasRet && fds->GetRetType() && !fds->GetRetType()->IsNothType()) {
        _diag.Report(Error, "function must return a value in all execution paths")
            .SetCode(ErrHasntRet)
            .AddSpan(fds->GetName().Start, fds->GetName().End);
    }
    else if (!hasRet && (!fds->GetRetType() || fds->GetRetType() && fds->GetRetType()->IsNothType())) {
        _builder.SetInsertionPoint(funcHir);
        _builder.CreateRet(new NothType(llvm::SMLoc(), llvm::SMLoc()), nullptr);
    }
    _builder.SetInsertionPoint(nullptr);
}

void
Semantic::analyzeUS(UsingStmt *us) {
    // TODO: implement
}

void
Semantic::analyzeRS(RetStmt *rs) {
    if (rs->GetExpr()) {
        auto valRes = analyzeExpr(rs->GetExpr());
        Value val = valRes.Val;
        if (!_funcsRetTypes.top()) {
            _funcsRetTypes.top() = val.Type;
        }
        else {
            valRes = implicitlyCast(valRes, &_funcsRetTypes.top());
        }
        _builder.CreateRet(_funcsRetTypes.top(), valRes.HirNode);
    }
    else {
        if (!_funcsRetTypes.top()) {
            _funcsRetTypes.top() = new NothType(llvm::SMLoc(), llvm::SMLoc());
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

Semantic::SemanticResult
Semantic::analyzeExpr(Expr *expr) {
    #define NODE(k, f, t) case k: return f(static_cast<t *>(expr));
    switch (expr->GetKind()) {
        NODE(NkBinaryExpr, analyzeBE, BinaryExpr);
        NODE(NkLitExpr, analyzeLE, LiteralExpr);
        NODE(NkUnaryExpr, analyzeUE, UnaryExpr);
        NODE(NkVarExpr, analyzeVE, VarExpr);
    }
    #undef NODE
}

Semantic::SemanticResult
Semantic::analyzeBE(BinaryExpr *be) {
    auto lhsRes = analyzeExpr(be->GetLHS());
    auto rhsRes = analyzeExpr(be->GetRHS());
    Value lhs = lhsRes.Val;
    Value rhs = rhsRes.Val;
    Type *commonType = getCommonTypeForOp(lhs.Type, rhs.Type, be->GetOp(), be->GetStartLoc(), be->GetEndLoc());
    lhsRes = implicitlyCast(lhsRes, &commonType);
    rhsRes = implicitlyCast(rhsRes, &commonType);

    HIRNode *binNode = _builder.CreateBinary(commonType, lhsRes.HirNode, rhsRes.HirNode, tokenKindToHIRBk(be->GetOp().Kind));

    if (lhs.IsUnknown() || rhs.IsUnknown()) {
        return { Value(Value::Unknown, ValueData(), commonType, be->GetStartLoc(), be->GetEndLoc()), binNode };
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
    
    switch (commonType->GetKind()) {
        #define VAL(t) Value(Value::Const, ValueData(static_cast<t>(res)), commonType, be->GetStartLoc(), be->GetEndLoc())
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
    _diag.Report(Error, "variable is undeclared in this scope")
        .SetCode(ErrUndeclaredVar)
        .AddSpan(ve->GetStartLoc(), ve->GetEndLoc());
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
            for (auto &t : tuple->GetTypes()) {
                resolveType(&t);
            }
            return *t;
        }
        case Type::Pointer: {
            Type **base = &(*t)->AsPointer()->GetBaseType();
            resolveType(base);
            return *t;
        }
        case Type::Array: {
            Type **base = &(*t)->AsArray()->GetBaseType();
            resolveType(base);
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

    if (src == dst) {
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
    }

    _diag.Report(Error, "cannot implicitly cast '" + src->ToString() + "' to '" + dst->ToString() + "'")
        .SetCode(ErrCannotImplCast)
        .AddSpan(res.Val.Start, res.Val.End);

    return { Value::GetIncorrectValue(), _builder.GetIncorrectValue() };
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