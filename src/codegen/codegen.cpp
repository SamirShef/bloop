#include <codegen/codegen.h>
#include <llvm/IR/Intrinsics.h>
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
        NODE(HIRNkDerefStore, generateDerefStore, HIRDerefStore);
        NODE(HIRNkDelStmt, generateDS, HIRDelStmt);
        NODE(HIRNkArrayStore, generateArrayStore, HIRArrayStore);
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
                case Parameter: {
                    auto *arg = _builder.GetInsertBlock()->getParent()->arg_begin() + ve->GetIndex();
                    if (arg->getType()->isPointerTy()) {
                        return arg;
                    }
                    else {
                        auto *alloca = _builder.CreateAlloca(arg->getType(), nullptr, arg->getName() + ".alloca");
                        return alloca;
                    }
                }
            }
        }
        case HIRNkFieldExpr: {
            auto fe = static_cast<HIRFieldExpr *>(node);
            llvm::Value *basePtr = generateLValue(fe->GetBase());
            return _builder.CreateStructGEP(getType(fe->GetBaseType()), basePtr, fe->GetIndex());
        }
        case HIRNkDeref: {
            auto deref = static_cast<HIRDeref *>(node);
            return generateExpr(deref->GetBase());
        }
        case HIRNkArrayAccessExpr: {
            auto aae = static_cast<HIRArrayAccessExpr *>(node);
            llvm::Value *index = generateExpr(aae->GetIndex());
            llvm::Value *basePtr = generateLValue(aae->GetBase());
            Type *baseType = aae->GetBaseType();
            
            if (aae->GetBaseType()->IsArray()) {
                llvm::Type *arrType = getType(baseType);
                return _builder.CreateInBoundsGEP(arrType, basePtr, { _builder.getInt64(0), index }, basePtr->getName() + ".idx." + index->getValueName()->first());
            } 
            else {
                llvm::Type *sliceType = getType(baseType);
                llvm::Value *dataFieldPtr = _builder.CreateStructGEP(sliceType, basePtr, 0, "slice.ptr.addr");
                llvm::Value *dataPtr = _builder.CreateLoad(_builder.getPtrTy(), dataFieldPtr, "slice.data.ptr");
                Type *elementType = baseType->AsSlice()->GetBaseType();
                return _builder.CreateInBoundsGEP(getType(elementType), dataPtr, index, "slice.element.addr");
            }
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
            _builder.CreateStore(arg, ptr);
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

llvm::Value *
CodeGen::generateDerefStore(HIRDerefStore *ds) {
    llvm::Value *ptr = generateExpr(ds->GetPtr());
    llvm::Value *val = generateExpr(ds->GetExpr());
    return _builder.CreateStore(val, ptr);
}

llvm::Value *
CodeGen::generateArrayStore(HIRArrayStore *as) {
    llvm::Value *index = generateExpr(as->GetIndex());
    llvm::Value *ptr = nullptr;
    
    if (as->GetBaseType()->IsArray()) {
        llvm::Value *basePtr = generateLValue(as->GetBase());
        llvm::Type *arrTy = getType(as->GetBaseType());
        ptr = _builder.CreateInBoundsGEP(arrTy, basePtr, { _builder.getInt64(0), index }, "array.store.gep");
    }
    else {
        llvm::Value *basePtr = generateExpr(as->GetBase());
        Type *elementType = as->GetBaseType()->AsSlice()->GetBaseType();
        ptr = _builder.CreateInBoundsGEP(getType(elementType), basePtr, index, "slice.store.gep");
    }
    
    llvm::Value *val = generateExpr(as->GetExpr());
    return _builder.CreateStore(val, ptr);
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
    Function &funcMeta = _funcsMap.at(fds->GetName());
    llvm::Function *func = funcMeta.Func;
    
    if (!fds->IsDeclaration()) {
        llvm::BasicBlock *init = llvm::BasicBlock::Create(_context, "init", func);
        _builder.SetInsertPoint(init);
    }
    int i = 0;
    for (auto &a : func->args()) {
        a.setName(fds->GetArgs()[i].Name);
        if (!fds->IsDeclaration()) {
            auto alloca = _builder.CreateAlloca(a.getType(), nullptr, a.getName());
            _builder.CreateStore(&a, alloca);
            funcMeta.Locals.push_back(alloca);
        }
        ++i;
    }

    if (fds->IsDeclaration()) {
        return nullptr;
    }

    _blockMap.clear();
    for (auto &bb : fds->GetBody()) {
        auto *llvmBB = llvm::BasicBlock::Create(_context, bb->GetName(), func);
        if (_blockMap.empty()) {
            _builder.CreateBr(llvmBB);
        }
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

llvm::Value *
CodeGen::generateDS(HIRDelStmt *ds) {
    llvm::FunctionType *freeType = llvm::FunctionType::get(_builder.getVoidTy(), { _builder.getPtrTy() }, false);
    auto free = _module->getOrInsertFunction("free", freeType);
    llvm::Value *val = generateExpr(ds->GetExpr());
    return _builder.CreateCall(free, { val }, "free." + val->getName());
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
        NODE(HIRNkDeref, generateDeref, HIRDeref);
        NODE(HIRNkRef, generateRef, HIRRef);
        NODE(HIRNkNilExpr, generateNE, HIRNilExpr);
        NODE(HIRNkNewExpr, generateNew, HIRNewExpr);
        NODE(HIRNkArrayInstanceExpr, generateAIE, HIRArrayInstanceExpr);
        NODE(HIRNkNilCheck, generateNilCheck, HIRNilCheck);
        NODE(HIRNkBoundsCheck, generateBoundsCheck, HIRBoundsCheck);
        NODE(HIRNkArrayAccessExpr, generateAAE, HIRArrayAccessExpr);

        case HIRNkVarDeclStmt: {
            auto vds = llvm::cast<HIRVarDeclStmt>(expr);
            auto var = _builder.CreateAlloca(getType(vds->GetType()), nullptr, vds->GetName());
            _builder.CreateStore(generateExpr(vds->GetExpr()), var);
            return var;
        }
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
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateUDiv(lhs, rhs, "udiv.tmp");
            }
            return _builder.CreateSDiv(lhs, rhs, "sdiv.tmp");
        case HIRBkRem:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFRem(lhs, rhs, "frem.tmp");
            }
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
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
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpUGT(lhs, rhs, "icmpugt.tmp");
            }
            return _builder.CreateICmpSGT(lhs, rhs, "icmpsgt.tmp");
        case HIRBkGtEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOGE(lhs, rhs, "fcmpoge.tmp");
            }
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpUGE(lhs, rhs, "icmpuge.tmp");
            }
            return _builder.CreateICmpSGE(lhs, rhs, "icmpsge.tmp");
        case HIRBkLt:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOLT(lhs, rhs, "fcmpolt.tmp");
            }
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
                return _builder.CreateICmpULT(lhs, rhs, "icmpult.tmp");
            }
            return _builder.CreateICmpSLT(lhs, rhs, "icmpslt.tmp");
        case HIRBkLtEq:
            if (be->GetCommonType()->IsFloating()) {
                return _builder.CreateFCmpOLE(lhs, rhs, "fcmpole.tmp");
            }
            if (be->GetCommonType()->IsInteger() && be->GetCommonType()->AsInteger()->IsUnsigned()) {
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
        case Type::Struct: {
            // TODO: implement
        }
        case Type::Trait: {
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
            if (ue->GetType()->IsFloating()) {
                return _builder.CreateFNeg(val, "fneg.tmp");
            }
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

llvm::Value *
CodeGen::generateDeref(HIRDeref *deref) {
    return _builder.CreateLoad(getType(deref->GetBaseType()), generateExpr(deref->GetBase()));
}

llvm::Value *
CodeGen::generateRef(HIRRef *ref) {
    return generateLValue(ref->GetBase());
}

llvm::Value *
CodeGen::generateNE(HIRNilExpr *ne) {
    return llvm::ConstantPointerNull::get(_builder.getPtrTy());
}

llvm::Value *
CodeGen::generateNew(HIRNewExpr *ne) {
    auto &dl = _module->getDataLayout();
    auto *llvmType = getType(ne->GetType());
    
    uint64_t headerSize = dl.getTypeAllocSize(llvmType);
    
    llvm::FunctionType *mallocType = llvm::FunctionType::get(_builder.getPtrTy(), { _builder.getInt64Ty() }, false);
    auto mallocFunc = _module->getOrInsertFunction("malloc", mallocType);

    llvm::Value *headerPtr = _builder.CreateCall(mallocFunc, { _builder.getInt64(headerSize) }, "new.header.ptr");

    if (ne->GetExpr()) {
        if (ne->GetType()->IsSlice()) {
            llvm::Value *arrayVal = generateExpr(ne->GetExpr());
            llvm::Type *arrayTy = arrayVal->getType();
            
            uint64_t arrayByteSize = dl.getTypeAllocSize(arrayTy);
            llvm::Value *dataPtr = _builder.CreateCall(mallocFunc, { _builder.getInt64(arrayByteSize) }, "new.slice.data");

            _builder.CreateStore(arrayVal, dataPtr);

            uint64_t arrayLen = arrayTy->getArrayNumElements();
            llvm::Value *sliceStruct = llvm::Constant::getNullValue(llvmType);
            sliceStruct = _builder.CreateInsertValue(sliceStruct, dataPtr, 0);
            sliceStruct = _builder.CreateInsertValue(sliceStruct, _builder.getInt64(arrayLen), 1);

            _builder.CreateStore(sliceStruct, headerPtr);}
        else {
            llvm::Value *val = generateExpr(ne->GetExpr());
            _builder.CreateStore(val, headerPtr);
        }
    }
    
    return headerPtr;
}

llvm::Value *
CodeGen::generateAIE(HIRArrayInstanceExpr *aie) {
    auto arrType = llvm::ArrayType::get(getType(aie->GetArrType()), aie->GetExprs().size());
    std::vector<llvm::Constant *> vals;
    for (auto e : aie->GetExprs()) {
        vals.push_back(llvm::cast<llvm::Constant>(generateExpr(e)));
    }
    llvm::Value *arr = llvm::ConstantArray::get(arrType, vals);
    return arr;
}

llvm::Value *
CodeGen::generateAAE(HIRArrayAccessExpr *aae) {
    llvm::Value *ptr = generateLValue(aae);
    
    Type *elementType = nullptr;
    if (aae->GetBaseType()->IsArray()) {
        elementType = aae->GetBaseType()->AsArray()->GetBaseType();
    }
    else {
        elementType = aae->GetBaseType()->AsSlice()->GetBaseType();
    }
    
    return _builder.CreateLoad(getType(elementType), ptr);
}

llvm::Value *
CodeGen::generateNilCheck(HIRNilCheck *nilCheck) {
    llvm::Value *ptrVal = generateExpr(nilCheck->GetPtr());

    llvm::Function *func = _builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *panicBB = llvm::BasicBlock::Create(_context, "nil.panic", func);
    llvm::BasicBlock *okBB = llvm::BasicBlock::Create(_context, "nil.ok", func);

    llvm::Value *isNull = _builder.CreateIsNull(ptrVal);
    _builder.CreateCondBr(isNull, panicBB, okBB);

    _builder.SetInsertPoint(panicBB);

    llvm::FunctionType *putsType = llvm::FunctionType::get(_builder.getInt32Ty(), { _builder.getPtrTy() }, false);
    llvm::FunctionCallee putsFunc = _module->getOrInsertFunction("puts", putsType);

    llvm::FunctionType *exitType = llvm::FunctionType::get(_builder.getVoidTy(), { _builder.getInt32Ty() }, false);
    llvm::FunctionCallee exitFunc = _module->getOrInsertFunction("exit", exitType);

    llvm::Value *panicMsg = _builder.CreateGlobalStringPtr(
        "\e[1;31mpanic\e[0m: runtime error: attempt to dereference a nil value at " + nilCheck->GetPos(),
        "panic.msg"
    );

    _builder.CreateCall(putsFunc, { panicMsg });
    _builder.CreateCall(exitFunc, { _builder.getInt32(1) });
    _builder.CreateUnreachable();

    _builder.SetInsertPoint(okBB);

    return ptrVal;
}

llvm::Value *
CodeGen::generateBoundsCheck(HIRBoundsCheck *boundsCheck) {
    llvm::Value *index = generateExpr(boundsCheck->GetIndex());
    llvm::Value *len = generateExpr(boundsCheck->GetLength());
    
    llvm::Function *func = _builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *panicBB = llvm::BasicBlock::Create(_context, "bounds.panic", func);
    llvm::BasicBlock *okBB = llvm::BasicBlock::Create(_context, "bounds.ok", func);

    llvm::Value *inBounds = _builder.CreateICmpULT(index, len, "is.in.bounds");
    _builder.CreateCondBr(inBounds, okBB, panicBB);

    _builder.SetInsertPoint(panicBB);

    llvm::FunctionType *putsType = llvm::FunctionType::get(_builder.getInt32Ty(), { _builder.getPtrTy() }, false);
    llvm::FunctionCallee putsFunc = _module->getOrInsertFunction("puts", putsType);

    llvm::FunctionType *exitType = llvm::FunctionType::get(_builder.getVoidTy(), { _builder.getInt32Ty() }, false);
    llvm::FunctionCallee exitFunc = _module->getOrInsertFunction("exit", exitType);

    llvm::Value *panicMsg = _builder.CreateGlobalStringPtr(
        "\e[1;31mpanic\e[0m: runtime error: index out of bounds at " + boundsCheck->GetPos(),
        "panic.bounds.msg"
    );

    _builder.CreateCall(putsFunc, { panicMsg });
    _builder.CreateCall(exitFunc, { _builder.getInt32(1) });
    _builder.CreateUnreachable();

    _builder.SetInsertPoint(okBB);

    return index;
}

llvm::Type *
CodeGen::getType(Type *type) {
    if (!type) {
        return _builder.getVoidTy();
    }
    
    switch (type->GetKind()) {
        case Type::Char: {
            return _builder.getInt32Ty();
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
        case Type::Slice: {
            std::string name = "slice." + type->AsSlice()->GetBaseType()->ToString();
            if (auto *s = llvm::StructType::getTypeByName(_context, name)) {
                return s;
            }
            return llvm::StructType::create(_context, { _builder.getPtrTy(), _builder.getInt64Ty() }, name);
        }
        case Type::Array: {
            auto *a = type->AsArray();
            return llvm::ArrayType::get(getType(a->GetBaseType()), a->GetSize());
        }
        case Type::Struct: {
            auto *s = type->AsStruct();
            return llvm::StructType::getTypeByName(_context, s->GetBaseMod()->ToString() + "." + s->GetName().Name);
        }
        case Type::Trait: {
            // TODO: implement
        }
        case Type::Noth: {
            return _builder.getVoidTy();
        }
    }
}

}
