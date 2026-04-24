#pragma once
#include <utils/modules/module.h>
#include <utils/types/types.h>
#include <llvm/Bitstream/BitstreamWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace bloop {

enum BlockIDs {
    ModBlockID = llvm::bitc::FIRST_APPLICATION_BLOCKID,
    StrPoolBlockID,
    TypePoolBlockID,
    ModPoolBlockID,
    SymbolsBlockID
};

enum TypeRecordIDs {
    TypeIdInteger = 1,
    TypeIdChar,
    TypeIdFloating,
    TypeIdPointer,
    TypeIdSlice,
    TypeIdTuple,
    TypeIdStruct,
    TypeIdTrait,
    TypeIdArray,
    TypeIdNoth,
};

enum RecordIDs {
    StrEntry = 1,
    ModPoolEntry,
    ModInfo,
    SymVar,
    SymFunc,
    SymStruct,
    SymTrait,
    SymPrimMethod
};

class Serializer {
    struct StringPool {
        std::vector<std::string> Strings;
        std::unordered_map<std::string, uint32_t> Map;

        uint32_t
        GetID(const std::string &s) {
            if (auto it = Map.find(s); it != Map.end()) {
                return it->second;
            }
            uint32_t id = Strings.size();
            Strings.push_back(s);
            Map[s] = id;
            return id;
        }
    };

    struct TypePool {
        std::vector<Type *> Types;
        std::unordered_map<Type *, uint32_t> Map;

        uint32_t
        GetID(Type *t) {
            if (!t) {
                return 0;
            }
            if (auto it = Map.find(t); it != Map.end()) {
                return it->second + 1;
            }

            switch (t->GetKind()) {
                case Type::Pointer:
                    GetID(llvm::cast<PointerType>(t)->GetBaseType());
                    break;
                case Type::Tuple:
                    for (Type *sub : llvm::cast<TupleType>(t)->GetTypes()) {
                        GetID(sub);
                    }
                    break;
                case Type::Array:
                    GetID(llvm::cast<ArrayType>(t)->GetBaseType());
                    break;
                default:
                    break;
            }

            uint32_t id = Types.size();
            Types.push_back(t);
            Map[t] = id;
            return id + 1; 
        }
    };

    struct ModulePool {
        std::vector<const Module *> Modules;
        std::unordered_map<const Module *, uint32_t> Map;

        uint32_t
        GetID(const Module *m) {
            if (!m) {
                return 0;
            }
            if (auto it = Map.find(m); it != Map.end()) {
                return it->second + 1;
            }

            if (m->Parent) {
                GetID(m->Parent);
            }

            uint32_t id = Modules.size();
            Modules.push_back(m);
            Map[m] = id;
            return id + 1;
        }
    };

    StringPool _strPool;
    ModulePool _modPool;
    TypePool   _typesPool;

public:
    void
    Serialize(const Module *root, const std::string &fileName) {
        std::error_code ec;
        llvm::raw_fd_ostream os(fileName, ec, llvm::sys::fs::OF_None);
        
        llvm::SmallVector<char, 0> buffer;
        llvm::BitstreamWriter w(buffer);

        // Imagine number 'BLB' (bloop bitcode)
        w.Emit((unsigned char)'B', 8);
        w.Emit((unsigned char)'L', 8);
        w.Emit((unsigned char)'B', 8);

        collectStringsAndTypes(root);
        writeStringPool(w);
        writeModulePool(w);
        writeTypePool(w);
        
        serializeModule(w, root);

        os.write((const char *)buffer.data(), buffer.size());
    }

private:
    void
    collectStringsAndTypes(const Module *mod) {
        _strPool.GetID(mod->Name);
        _modPool.GetID(mod);
        for (auto &[name, var] : mod->Vars) {
            _strPool.GetID(var.Name.Name);
            _typesPool.GetID(var.Type);
        }
        for (auto &[name, candidates] : mod->FuncOverloads) {
            for (auto &func : candidates.Candidates) {
                _strPool.GetID(func.Name.Name);
                _typesPool.GetID(func.RetType);
                for (auto &a : func.Args) {
                    _typesPool.GetID(a.Type);
                }
            }
        }
        for (auto &[name, s] : mod->Structs) {
            _strPool.GetID(s.Name.Name);
            for (auto &f : s.Fields) {
                _strPool.GetID(f.Var.Name.Name);
                _typesPool.GetID(f.Var.Type);
            }
            for (auto &overload : s.Methods) {
                for (auto &m : overload.Candidates) {
                    _strPool.GetID(m.Func.Name.Name);
                    _typesPool.GetID(m.Func.RetType);
                    for (auto &a : m.Func.Args) {
                        _typesPool.GetID(a.Type);
                    }
                }
            }
        }
        for (auto &[type, overloads] : mod->PrimitivesMethods) {
            _typesPool.GetID(type);
            for (auto &overload : overloads) {
                for (auto &m : overload.Candidates) {
                    _strPool.GetID(m.Func.Name.Name);
                    _typesPool.GetID(m.Func.RetType);
                    _modPool.GetID(m.Func.Parent);
                    for (auto &a : m.Func.Args) {
                        _typesPool.GetID(a.Type);
                    }
                }
            }
        }
        for (auto &[name, mod] : mod->Imports) {
            collectStringsAndTypes(mod);
        }
        for (auto &[name, mod] : mod->Submods) {
            collectStringsAndTypes(mod);
        }
    }

    void
    writeStringPool(llvm::BitstreamWriter &w) {
        w.EnterSubblock(StrPoolBlockID, 3);
        for (const auto &s : _strPool.Strings) {
            llvm::SmallVector<uint64_t, 64> record;
            for (char c : s) {
                record.push_back(static_cast<uint64_t>(c));
            }
            w.EmitRecord(StrEntry, record);
        }
        w.ExitBlock();
    }

    void
    writeModulePool(llvm::BitstreamWriter &w) {
        if (_modPool.Modules.empty()) {
            return;
        }
        w.EnterSubblock(ModPoolBlockID, 3);
        
        for (const Module *m : _modPool.Modules) {
            llvm::SmallVector<uint64_t, 2> record = {
                static_cast<uint64_t>(_strPool.GetID(m->Name)),
                static_cast<uint64_t>(_modPool.GetID(m->Parent))
            };
            w.EmitRecord(ModPoolEntry, record);
        }
        w.ExitBlock();
    }

    void
    writeTypePool(llvm::BitstreamWriter &w) {
        if (_typesPool.Types.empty()) {
            return;
        }
        w.EnterSubblock(TypePoolBlockID, 4);

        for (Type *t : _typesPool.Types) {
            llvm::SmallVector<uint64_t, 8> record;
            
            switch (t->GetKind()) {
                case Type::Integer: {
                    auto *it = llvm::cast<IntegerType>(t);
                    record = { it->GetBitWidth(), it->IsUnsigned() };
                    w.EmitRecord(TypeIdInteger, record);
                    break;
                }
                case Type::Floating: {
                    auto *ft = llvm::cast<FloatingType>(t);
                    record = { static_cast<uint64_t>(ft->IsDouble() ? 1 : 0) };
                    w.EmitRecord(TypeIdFloating, record);
                    break;
                }
                case Type::Pointer: {
                    auto *pt = llvm::cast<PointerType>(t);
                    record = { _typesPool.GetID(pt->GetBaseType()) };
                    w.EmitRecord(TypeIdPointer, record);
                    break;
                }
                case Type::Slice: {
                    auto *st = llvm::cast<SliceType>(t);
                    record = { _typesPool.GetID(st->GetBaseType()) };
                    w.EmitRecord(TypeIdSlice, record);
                    break;
                }
                case Type::Struct: {
                    auto *st = llvm::cast<StructType>(t);
                    record = { _strPool.GetID(st->GetName().Name), static_cast<uint64_t>(_modPool.GetID(st->GetBaseMod())) };
                    w.EmitRecord(TypeIdStruct, record);
                    break;
                }
                case Type::Trait: {
                    auto *trt = llvm::cast<TraitType>(t);
                    record = { _strPool.GetID(trt->GetName().Name), static_cast<uint64_t>(_modPool.GetID(trt->GetBaseMod())) };
                    w.EmitRecord(TypeIdTrait, record);
                    break;
                }
                case Type::Tuple: {
                    auto *tt = llvm::cast<TupleType>(t);
                    record.push_back(tt->GetTypesCount());
                    for (Type *sub : tt->GetTypes()) {
                        record.push_back(_typesPool.GetID(sub));
                    }
                    w.EmitRecord(TypeIdTuple, record);
                    break;
                }
                case Type::Noth: {
                    w.EmitRecord(TypeIdNoth, record);
                }
            }
        }
        w.ExitBlock();
    }

    void
    serializeModule(llvm::BitstreamWriter &w, const Module *mod) {
        w.EnterSubblock(ModBlockID, 4);
        
        llvm::SmallVector<uint64_t, 8> modRec = {
            static_cast<uint64_t>(_strPool.GetID(mod->Name)),
            static_cast<uint64_t>(mod->Access)
        };
        w.EmitRecord(ModInfo, modRec);

        serializeVars(w, mod->Vars);
        serializeFuncs(w, mod->FuncOverloads);
        serializeStructs(w, mod->Structs);
        serializePrimitiveMethods(w, mod->PrimitivesMethods);

        for (auto &[name, mod] : mod->Imports) {
            serializeModule(w, mod);
        }
        for (auto &[name, mod] : mod->Submods) {
            serializeModule(w, mod);
        }

        w.ExitBlock();
    }

    void
    serializeVars(llvm::BitstreamWriter &w, const std::unordered_map<std::string, Variable> &vars) {
        for (auto &[name, v] : vars) {
            llvm::SmallVector<uint64_t, 8> varRec = {
                static_cast<uint64_t>(_strPool.GetID(v.Name.Name)),
                static_cast<uint64_t>(_typesPool.GetID(v.Type)),
                static_cast<uint64_t>(v.IsConst),
                static_cast<uint64_t>(v.Access),
                static_cast<uint64_t>(v.Storage),
                static_cast<uint64_t>(v.Index)
            };
            w.EmitRecord(SymVar, varRec);
        }
    }

    void
    serializeFuncs(llvm::BitstreamWriter &w, const std::unordered_map<std::string, FuncOverload> &overloads) {
        for (auto &[name, candidates] : overloads) {
            for (auto &f : candidates.Candidates) {
                llvm::SmallVector<uint64_t, 8> funcRec = {
                    static_cast<uint64_t>(_strPool.GetID(f.Name.Name)),
                    static_cast<uint64_t>(_typesPool.GetID(f.RetType)),
                    static_cast<uint64_t>(f.Access),
                    static_cast<uint64_t>(f.Storage),
                    static_cast<uint64_t>(f.Args.size())
                };
                for (auto &a : f.Args) {
                    funcRec.push_back(static_cast<uint64_t>(_typesPool.GetID(a.Type)));
                }
                w.EmitRecord(SymFunc, funcRec);
            }
        }
    }

    void
    serializeStructs(llvm::BitstreamWriter &w, const std::unordered_map<std::string, Struct> &structs) {
        for (auto &[name, s] : structs) {
            llvm::SmallVector<uint64_t, 64> structRec = {
                static_cast<uint64_t>(_strPool.GetID(name)),
                static_cast<uint64_t>(s.Access),
                static_cast<uint64_t>(_modPool.GetID(s.Parent)),
                static_cast<uint64_t>(s.Fields.size())
            };
            for (auto &f : s.Fields) {
                structRec.push_back(static_cast<uint64_t>(_strPool.GetID(f.Var.Name.Name)));
                structRec.push_back(static_cast<uint64_t>(_typesPool.GetID(f.Var.Type)));
                structRec.push_back(static_cast<uint64_t>(f.Access));
                structRec.push_back(static_cast<uint64_t>(f.IsStatic));
            }
            structRec.push_back(static_cast<uint64_t>(s.Methods.size()));
            for (auto &overload : s.Methods) {
                structRec.push_back(static_cast<uint64_t>(overload.Candidates.size()));
                for (auto &m : overload.Candidates) {
                    structRec.push_back(static_cast<uint64_t>(m.IsStatic));
                    structRec.push_back(static_cast<uint64_t>(m.Access));
                    structRec.push_back(static_cast<uint64_t>(_strPool.GetID(m.Func.Name.Name)));
                    structRec.push_back(static_cast<uint64_t>(_typesPool.GetID(m.Func.RetType)));
                    structRec.push_back(static_cast<uint64_t>(m.Func.Storage));
                    
                    structRec.push_back(static_cast<uint64_t>(m.Func.Args.size()));
                    for (auto &a : m.Func.Args) {
                        structRec.push_back(static_cast<uint64_t>(_typesPool.GetID(a.Type)));
                    }
                }
            }
            w.EmitRecord(SymStruct, structRec);
        }
    }

    void
    serializePrimitiveMethods(llvm::BitstreamWriter &w, const std::unordered_map<Type *, std::vector<MethodOverload>> &methods) {
        for (auto &[type, overloads] : methods) {
            llvm::SmallVector<uint64_t, 64> record = {
                static_cast<uint64_t>(_typesPool.GetID(type)),
                static_cast<uint64_t>(overloads.size())
            };

            for (auto &overload : overloads) {
                record.push_back(static_cast<uint64_t>(overload.Candidates.size()));
                for (auto &m : overload.Candidates) {
                    record.push_back(static_cast<uint64_t>(m.IsStatic));
                    record.push_back(static_cast<uint64_t>(m.Access));
                    record.push_back(static_cast<uint64_t>(_strPool.GetID(m.Func.Name.Name)));
                    record.push_back(static_cast<uint64_t>(_typesPool.GetID(m.Func.RetType)));
                    record.push_back(static_cast<uint64_t>(m.Func.Storage));
                    record.push_back(static_cast<uint64_t>(_modPool.GetID(m.Func.Parent)));
                    
                    record.push_back(static_cast<uint64_t>(m.Func.Args.size()));
                    for (auto &a : m.Func.Args) {
                        record.push_back(static_cast<uint64_t>(_typesPool.GetID(a.Type)));
                    }
                }
            }
            w.EmitRecord(SymPrimMethod, record);
        }
    }
};

}