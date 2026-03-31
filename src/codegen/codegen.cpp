#include <codegen/codegen.h>
#include <utils/types/types.h>

namespace bloop {

void
CodeGen::generateNode(HIRNode *node) {
    if (!node) {
        return;
    }

    #define NODE(k, f, t) case k: return f(static_cast<t *>(node));
    switch (node->GetKind()) {
        NODE(HIRNkVarDeclStmt, generateVDS, HIRVarDeclStmt);
        NODE(HIRNkFuncDeclStmt, generateFDS, HIRFuncDeclStmt);
        NODE(HIRNkRetStmt, generateRS, HIRRetStmt);
    }
    #undef NODE
}

void
CodeGen::generateVDS(HIRVarDeclStmt *vds) {
    llvm::Value *var = nullptr;
    llvm::Value *val = vds->GetExpr() ? generateExpr(vds->GetExpr()) : llvm::ConstantExpr::getNullValue(getType(vds->GetType()));
    switch (vds->GetStorageKind()) {
        case Static: {
            var = new llvm::GlobalVariable(*_module, getType(vds->GetType()), vds->IsConst(), llvm::GlobalValue::ExternalLinkage,
                                           llvm::cast<llvm::Constant>(val), vds->GetName());
            _globals.push_back(llvm::cast<llvm::GlobalVariable>(var));
            break;
        }
        case Stack: {
            var = _builder.CreateAlloca(getType(vds->GetType()), nullptr, vds->GetName());
            _builder.CreateStore(val, var);
            auto funcName = _builder.GetInsertBlock()->getParent()->getName().str();
            auto &func = _funcs.at(funcName);
            func.Locals.push_back(llvm::cast<llvm::AllocaInst>(var));
            break;
        }
    }
}

void
CodeGen::generateFDS(HIRFuncDeclStmt *fds) {
    std::string name = fds->GetName();
    for (auto &a : fds->GetArgs()) {
        name += a.Type->ToString();
    }
    std::vector<llvm::Type *> args(fds->GetArgs().size());
    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        args[i] = getType(fds->GetArgs()[i].Type);
    }
    llvm::FunctionType *funcType = llvm::FunctionType::get(getType(fds->GetRetType()), args, false);
    llvm::Function *func = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, name, *_module);
    _funcs.emplace(name, Function { func });

    int i = 0;
    for (auto &a : func->args()) {
        a.setName(fds->GetArgs()[i].Name);
        ++i;
    }

    llvm::BasicBlock *entry = llvm::BasicBlock::Create(_context, "entry", func);
    _builder.SetInsertPoint(entry);
    for (auto &s : fds->GetBody()) {
        generateNode(s);
    }
}

void
CodeGen::generateRS(HIRRetStmt *rs) {
    if (rs->GetExpr()) {
        _builder.CreateRet(generateExpr(rs->GetExpr()));
    }
    else {
        _builder.CreateRetVoid();
    }
}

llvm::Value *
CodeGen::generateExpr(HIRNode *expr) {
    #define NODE(k, f, t) case k: return f(static_cast<t *>(expr));
    switch (expr->GetKind()) {
        NODE(HIRNkBinaryExpr, generateBE, HIRBinaryExpr);
        NODE(HIRNkLitExpr, generateLE, HIRLiteralExpr);
        NODE(HIRNkUnaryExpr, generateUE, HIRUnaryExpr);
        NODE(HIRNkVarExpr, generateVE, HIRVarExpr);
        NODE(HIRNkCast, generateCast, HIRCastNode);
    }
    #undef NODE
}

llvm::Value *
CodeGen::generateBE(HIRBinaryExpr *be) {
    llvm::Value *lhs = generateExpr(be->GetLHS());
    llvm::Value *rhs = generateExpr(be->GetRHS());
    llvm::Type *type = getType(be->GetCommonType());
    switch (be->GetOp()) {
        case HIRBkAdd:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFAdd(lhs, rhs, "fadd.tmp");
            }
            return _builder.CreateAdd(lhs, rhs, "add.tmp");
        case HIRBkSub:
            return _builder.CreateSub(lhs, rhs, "sub.tmp");
        case HIRBkMul:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFMul(lhs, rhs, "fmul.tmp");
            }
            return _builder.CreateMul(lhs, rhs, "mul.tmp");
        case HIRBkDiv:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFDiv(lhs, rhs, "fdiv.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateUDiv(lhs, rhs, "udiv.tmp");
            }
            return _builder.CreateSDiv(lhs, rhs, "sdiv.tmp");
        case HIRBkRem:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFRem(lhs, rhs, "frem.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateURem(lhs, rhs, "urem.tmp");
            }
            return _builder.CreateSRem(lhs, rhs, "srem.tmp");
        case HIRBkEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOEQ(lhs, rhs, "fcmpoeq.tmp");
            }
            return _builder.CreateICmpEQ(lhs, rhs, "icmpeq.tmp");
        case HIRBkNEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpONE(lhs, rhs, "fcmpone.tmp");
            }
            return _builder.CreateICmpNE(lhs, rhs, "icmpne.tmp");
        case HIRBkGt:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOGT(lhs, rhs, "fcmpogt.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpUGT(lhs, rhs, "icmpugt.tmp");
            }
            return _builder.CreateICmpSGT(lhs, rhs, "icmpsgt.tmp");
        case HIRBkGtEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOGE(lhs, rhs, "fcmpoge.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpUGE(lhs, rhs, "icmpuge.tmp");
            }
            return _builder.CreateICmpSGE(lhs, rhs, "icmpsge.tmp");
        case HIRBkLt:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOLT(lhs, rhs, "fcmpolt.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpULT(lhs, rhs, "icmpult.tmp");
            }
            return _builder.CreateICmpSLT(lhs, rhs, "icmpslt.tmp");
        case HIRBkLtEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOLE(lhs, rhs, "fcmpole.tmp");
            }
            if (be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpULE(lhs, rhs, "icmpule.tmp");
            }
            return _builder.CreateICmpSLE(lhs, rhs, "icmpsle.tmp");
        case HIRBkAnd:
            return _builder.CreateLogicalAnd(lhs, rhs);
        case HIRBkOr:
            return _builder.CreateLogicalOr(lhs, rhs);
    }
}

llvm::Value *
CodeGen::generateLE(HIRLiteralExpr *le) {
    auto type = le->GetVal().Type;
    auto data = le->GetVal().Data;
    switch (type->GetKind()) {
        case Type::Char: {
            // TODO: implement
        }
        case Type::Integer: {
            return _builder.getIntN(type->AsInteger()->GetBitWidth(), std::get<0>(data));
        }
        case Type::Size: {
            return _builder.getInt64(std::get<0>(data));
        }
        case Type::Floating: {
            return llvm::ConstantFP::get(getType(type), std::get<1>(data));
        }
        case Type::Tuple: {
            // TODO: implement
        }
        case Type::String: {
            return _builder.CreateGlobalString(std::get<2>(data), "string.lit", 0, _module);
        }
        case Type::Pointer: {
            // TODO: implement
        }
        case Type::Array: {
            // TODO: implement
        }
        case Type::StructPtr: {
            // TODO: implement
        }
        case Type::TraitPtr: {
            // TODO: implement
        }
        case Type::ModulePtr: {
            // TODO: implement
        }
        case Type::Noth: {
            // TODO: implement
        }
    }
}

llvm::Value *
CodeGen::generateUE(HIRUnaryExpr *ue) {
    llvm::Value *val = generateExpr(ue->GetRHS());
    switch (ue->GetOp()) {
        case HIRUkMinus:
            return _builder.CreateNeg(val, "neg.tmp");
        case HIRUkNot:
            return _builder.CreateNot(val, "not.tmp");
    }
}

llvm::Value *
CodeGen::generateVE(HIRVarExpr *ve) {
    switch (ve->GetStorageKind()) {
        case Static: {
            auto *var = _globals[ve->GetIndex()];
            if (_builder.GetInsertBlock()) {
                auto *load = _builder.CreateLoad(var->getValueType(), var, var->getName() + ".load");
                return load;
            }
            return var->getInitializer();
        }
        case Stack: {
            auto funcName = _builder.GetInsertBlock()->getParent()->getName().str();
            auto func = _funcs.at(funcName);
            auto *var = func.Locals[ve->GetIndex()];
            auto *load = _builder.CreateLoad(var->getAllocatedType(), var, var->getName() + ".load");
            return load;
        }
        case Parameter: {
            auto args = _builder.GetInsertBlock()->getParent()->args();
            auto arg = args.begin() + ve->GetIndex();
            return arg;
        }
    }
}

llvm::Value *
CodeGen::generateCast(HIRCastNode *cast) {
    llvm::Value *expr = generateExpr(cast->GetExpr());
    switch (cast->GetCastKind()) {
        case IntToFloat:
            if (cast->GetFromType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateUIToFP(expr, getType(cast->GetToType()), "uitofp.tmp");
            }
            return _builder.CreateSIToFP(expr, getType(cast->GetToType()), "sitofp.tmp");
        case FloatToInt:
            if (cast->GetToType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateFPToUI(expr, getType(cast->GetToType()), "fptoui.tmp");
            }
            return _builder.CreateFPToSI(expr, getType(cast->GetToType()), "fptosi.tmp");
        case SignExtend:
            return _builder.CreateSExt(expr, getType(cast->GetToType()), "sext.tmp");
        case ZeroExtend:
            return _builder.CreateZExt(expr, getType(cast->GetToType()), "zext.tmp");
        case Truncate:
            if (cast->GetFromType()->AsFloating()) {
                return _builder.CreateFPTrunc(expr, getType(cast->GetToType()), "fptrunc.tmp");
            }
            return _builder.CreateTrunc(expr, getType(cast->GetToType()), "trunc.tmp");
        case Bitcast:
            return _builder.CreateBitCast(expr, getType(cast->GetToType()), "bitcast.tmp");
    }
}

llvm::Type *
CodeGen::getType(Type *type) {
    if (!type) {
        return _builder.getVoidTy();
    }
    
    switch (type->GetKind()) {
        case Type::Char: {
            // TODO: implement
        }
        case Type::Integer: {
            return _builder.getIntNTy(type->AsInteger()->GetBitWidth());
        }
        case Type::Size: {
            return _builder.getInt64Ty();
        }
        case Type::Floating: {
            auto *ty = type->AsFloating();
            if (ty->IsFloat()) {
                return _builder.getFloatTy();
            }
            else {
                return _builder.getDoubleTy();
            }
        }
        case Type::Tuple: {
            // TODO: implement
        }
        case Type::String: {
            // TODO: implement
        }
        case Type::Pointer: {
            return _builder.getPtrTy();
        }
        case Type::Array: {
            // TODO: implement
        }
        case Type::StructPtr: {
            // TODO: implement
        }
        case Type::TraitPtr: {
            // TODO: implement
        }
        case Type::ModulePtr: {
            // TODO: implement
        }
        case Type::Noth: {
            return _builder.getVoidTy();
        }
    }
}

}