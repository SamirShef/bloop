#include <codegen/codegen.h>
#include <utils/types/types.h>

namespace bloop {

llvm::Value *
CodeGen::generateNode(HIRNode *node) {
    if (!node) {
        return nullptr;
    }

    #define NODE(k, f, t) case k: return f(static_cast<t *>(node));
    switch (node->GetKind()) {
        NODE(HIRNkVarDeclStmt, generateVDS, HIRVarDeclStmt);
        NODE(HIRNkVarStore, generateVarStore, HIRVarStore);
        NODE(HIRNkFieldStore, generateFieldStore, HIRFieldStore);
        NODE(HIRNkFuncDeclStmt, generateFDS, HIRFuncDeclStmt);
        NODE(HIRNkFuncCallExpr, generateFCE, HIRFuncCallExpr);
        NODE(HIRNkRetStmt, generateRS, HIRRetStmt);
        NODE(HIRNkBasicBlock, generateBB, HIRBasicBlock);
        NODE(HIRNkBranch, generateBR, HIRBranch);
        NODE(HIRNkStructDeclStmt, generateSDS, HIRStructDeclStmt);
    }
    #undef NODE
}

llvm::Value *
CodeGen::generateLValue(HIRNode *node) {
    switch (node->GetKind()) {
        case HIRNkVarExpr: {
            auto ve = static_cast<HIRVarExpr *>(node);
            switch (ve->GetStorageKind()) {
                case Static: {
                    return _globals[ve->GetIndex()];
                }
                case Stack: {
                    auto funcName = _builder.GetInsertBlock()->getParent()->getName().str();
                    return _funcsMap.at(funcName).Locals[ve->GetIndex()];
                }
            }
        }
        case HIRNkFieldExpr: {
            auto fe = static_cast<HIRFieldExpr *>(node);
            llvm::Value *basePtr = generateLValue(fe->GetBase());
            return _builder.CreateStructGEP(getType(fe->GetBaseType()), basePtr, fe->GetIndex());
        }
        default:
            return nullptr;
    }
}

llvm::Value *
CodeGen::generateVDS(HIRVarDeclStmt *vds) {
    llvm::Value *var = nullptr;
    llvm::Value *val = vds->GetExpr() ? generateExpr(vds->GetExpr()) : (vds->GetStorageKind() == Extern ? nullptr : llvm::ConstantExpr::getNullValue(getType(vds->GetType())));
    switch (vds->GetStorageKind()) {
        case Extern:
        case Static: {
            var = new llvm::GlobalVariable(*_module, getType(vds->GetType()), vds->IsConst(), llvm::GlobalValue::ExternalLinkage,
                                           val ? llvm::cast<llvm::Constant>(val) : nullptr, vds->GetName());
            _globals.push_back(llvm::cast<llvm::GlobalVariable>(var));
            break;
        }
        case Stack: {
            var = _builder.CreateAlloca(getType(vds->GetType()), nullptr, vds->GetName());
            _builder.CreateStore(val, var);
            auto funcName = _builder.GetInsertBlock()->getParent()->getName().str();
            auto &func = _funcsMap.at(funcName);
            func.Locals.push_back(llvm::cast<llvm::AllocaInst>(var));
            break;
        }
    }
    return var;
}

llvm::Value *
CodeGen::generateVarStore(HIRVarStore *varStore) {
    llvm::Value *ptr = nullptr;
    switch (varStore->GetStorageKind()) {
        case Static: {
            ptr = _globals[varStore->GetIndex()];
            break;
        }
        case Stack: {
            auto funcName = _builder.GetInsertBlock()->getParent()->getName().str();
            auto func = _funcsMap.at(funcName);
            ptr = func.Locals[varStore->GetIndex()];
            break;
        }
        case Parameter: {
            auto args = _builder.GetInsertBlock()->getParent()->args();
            auto arg = args.begin() + varStore->GetIndex();
            ptr = _builder.CreateAlloca(arg->getType(), nullptr, arg->getName() + ".alloca");
            break;
        }
    }
    return _builder.CreateStore(generateExpr(varStore->GetExpr()), ptr);
}

llvm::Value *
CodeGen::generateFieldStore(HIRFieldStore *fieldStore) {
    llvm::Value *base = generateLValue(fieldStore->GetBase());
    llvm::Value *fieldPtr = _builder.CreateStructGEP(getType(fieldStore->GetBaseType()), base, fieldStore->GetIndex(), base->getName() + "." + std::to_string(fieldStore->GetIndex()) + ".gep");
    return _builder.CreateStore(generateExpr(fieldStore->GetExpr()), fieldPtr);
}

void
CodeGen::declareFDS(HIRFuncDeclStmt *fds) {
    if (_funcsMap.count(fds->GetName())) {
        return;
    }

    std::vector<llvm::Type *> args(fds->GetArgs().size());
    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        args[i] = getType(fds->GetArgs()[i].Type);
    }
    
    llvm::FunctionType *funcType = llvm::FunctionType::get(getType(fds->GetRetType()), args, false);
    llvm::Function *func = llvm::Function::Create(funcType, llvm::GlobalValue::ExternalLinkage, fds->GetName(), *_module);
    
    _funcsMap.emplace(fds->GetName(), Function { func });
    _funcs.push_back(func);

    if (fds->IsMain()) {
        _userMainFunc = func;
    }
}

llvm::Value *
CodeGen::generateFDS(HIRFuncDeclStmt *fds) {
    llvm::Function *func = _funcsMap.at(fds->GetName()).Func;
    
    int i = 0;
    for (auto &a : func->args()) {
        a.setName(fds->GetArgs()[i].Name);
        ++i;
    }

    if (fds->IsDeclaration()) {
        return nullptr;
    }

    _blockMap.clear();
    for (auto &bb : fds->GetBody()) {
        auto *llvmBB = llvm::BasicBlock::Create(_context, bb->GetName(), func);
        _blockMap[bb] = llvmBB;
    }

    for (auto &bb : fds->GetBody()) {
        generateBB(bb);
    }
    return func;
}

llvm::Value *
CodeGen::generateRS(HIRRetStmt *rs) {
    if (rs->GetExpr()) {
        return _builder.CreateRet(generateExpr(rs->GetExpr()));
    }
    else {
        return _builder.CreateRetVoid();
    }
}

llvm::Value *
CodeGen::generateBB(HIRBasicBlock *bb) {
    llvm::BasicBlock *block = _blockMap.at(bb);
    _builder.SetInsertPoint(block);
    for (auto &s : bb->GetInstructions()) {
        generateNode(s);
    }
    return block;
}

llvm::Value *
CodeGen::generateBR(HIRBranch *br) {
    if (br->GetCond()) {
        return _builder.CreateCondBr(generateExpr(br->GetCond()), _blockMap.at(br->GetThen()), _blockMap.at(br->GetElse()));
    }
    return _builder.CreateBr(_blockMap.at(br->GetThen()));
}

llvm::Value *
CodeGen::generateSDS(HIRStructDeclStmt *sds) {
    std::vector<llvm::Type *> fields;
    for (auto &f : sds->GetFields()) {
        fields.push_back(getType(f));
    }
    llvm::StructType *s = llvm::StructType::create(_context, fields, sds->GetName());
    return nullptr;
}

void
CodeGen::generateImplicitMain() {
    llvm::Type *argcType = _builder.getInt32Ty();
    llvm::Type *argvType = _builder.getPtrTy();
    
    llvm::FunctionType *mainType = llvm::FunctionType::get(argcType, { argcType, argvType }, false);
    llvm::Function *main = llvm::Function::Create(mainType, llvm::GlobalValue::ExternalLinkage, "main", *_module);
    main->arg_begin()->setName("argc");
    (main->arg_begin() + 1)->setName("argv");
    
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(_context, "entry", main);
    _builder.SetInsertPoint(entry);
    llvm::Value *retVal = _builder.CreateCall(_userMainFunc, { main->arg_begin(), main->arg_begin() + 1 });
    
    if (_userMainFunc->getReturnType()->isVoidTy()) {
        _builder.CreateRet(_builder.getInt32(0));
    }
    else {
        if (_userMainFunc->getReturnType()->getIntegerBitWidth() > 32) {
            retVal = _builder.CreateTrunc(retVal, argcType, "trunc.tmp");
        }
        else if (_userMainFunc->getReturnType()->getIntegerBitWidth() < 32) {
            retVal = _builder.CreateSExt(retVal, argcType, "sext.tmp");
        }
        _builder.CreateRet(retVal);
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
        NODE(HIRNkFuncCallExpr, generateFCE, HIRFuncCallExpr);
        NODE(HIRNkStructInstanceExpr, generateSIE, HIRStructInstanceExpr);
        NODE(HIRNkFieldExpr, generateFE, HIRFieldExpr);
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
            auto func = _funcsMap.at(funcName);
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
        case FPExtend:
            return _builder.CreateFPExt(expr, getType(cast->GetToType()), "fpext.tmp");
    }
}

llvm::Value *
CodeGen::generateFCE(HIRFuncCallExpr *fce) {
    llvm::Function *func = _module->getFunction(fce->GetName());
    std::vector<llvm::Value *> args(fce->GetArgs().size());

    for (int i = 0; i < args.size(); ++i) {
        args[i] = generateExpr(fce->GetArgs()[i]);
    }
    
    return _builder.CreateCall(func, args, fce->GetName() + ".call");
}

llvm::Value *
CodeGen::generateSIE(HIRStructInstanceExpr *sie) {
    llvm::StructType *s = llvm::StructType::getTypeByName(_context, sie->GetName());
    if (_builder.GetInsertBlock()) {
        llvm::AllocaInst *alloca = _builder.CreateAlloca(s, nullptr, s->getName() + ".alloca");
        for (const auto &f : sie->GetFields()) {
            std::string name = s->getName().str() + "." + std::to_string(f.first);
            llvm::Value *fieldPtr = _builder.CreateStructGEP(s, alloca, f.first, name + ".gep");
            llvm::Value *val = generateExpr(f.second);
            _builder.CreateStore(val, fieldPtr);
        }
        return _builder.CreateLoad(s, alloca, s->getName() + ".alloca.load");
    }
    else {
        std::vector<llvm::Constant *> fieldValues;
        for (const auto &f : sie->GetFields()) {
            llvm::Value *val = generateExpr(f.second);
            fieldValues.push_back(llvm::cast<llvm::Constant>(val));
        }
        return llvm::ConstantStruct::get(s, fieldValues);
    }
}

llvm::Value *
CodeGen::generateFE(HIRFieldExpr *fe) {
    llvm::Value *base = generateExpr(fe->GetBase());
    return _builder.CreateExtractValue(base, fe->GetIndex(), base->getName() + "." + std::to_string(fe->GetIndex()));
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
            auto *s = type->AsStructPtr();
            return llvm::StructType::getTypeByName(_context, s->GetBaseMod()->ToString() + "." + s->GetName().Name);
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