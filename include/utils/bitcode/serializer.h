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
    SymbolsBlockID
};

enum TypeRecordIDs {
    TypeIdInteger = 1,
    TypeIdChar,
    TypeIdFloating,
    TypeIdPointer,
    TypeIdTuple,
    TypeIdStruct,
    TypeIdTrait,
    TypeIdArray,
};

enum RecordIDs {
    // String Pool
    StrEntry    = 1,
    // Module
    ModInfo     = 1,
    // Symbols
    SymVar      = 1,
    SymFunc     = 2,
    SymStruct   = 3,
    SymTrait    = 4
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
        Serializer *Ser;

        explicit TypePool(Serializer *s) : Ser(s) {}

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

    StringPool _strPool;
    TypePool   _typesPool{ this };

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

        collectStrings(root);
        writeStringPool(w);
        serializeModule(w, root);

        os.write((const char *)buffer.data(), buffer.size());
    }

    void
    CollectStringsAndTypes(const Module *mod) {
        _strPool.GetID(mod->Name);
        for (auto &[name, var] : mod->Vars) {
            _strPool.GetID(var.Name.Name);
            _typesPool.GetID(var.Type);
        }
        for (auto &[name, sub] : mod->Submods) {
            CollectStringsAndTypes(sub);
        }
    }

    void
    WriteTypePool(llvm::BitstreamWriter &w) {
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
                case Type::StructPtr: {
                    auto *st = llvm::cast<StructType>(t);
                    record = { _strPool.GetID(st->GetName().Name) };
                    w.EmitRecord(TypeIdStruct, record);
                    break;
                }
                case Type::TraitPtr: {
                    auto *trt = llvm::cast<TraitType>(t);
                    record = { _strPool.GetID(trt->GetName().Name) };
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
            }
        }
        w.ExitBlock();
    }

private:
    void
    collectStrings(const Module *mod) {
        _strPool.GetID(mod->Name);
        for (auto &[name, var] : mod->Vars) {
            _strPool.GetID(var.Name.Name);
            _strPool.GetID(var.Type->ToString());
        }

        for (auto &[name, sub] : mod->Submods) {
            collectStrings(sub);
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
    serializeModule(llvm::BitstreamWriter &w, const Module *mod) {
        w.EnterSubblock(ModBlockID, 4);
        
        llvm::SmallVector<uint64_t, 8> modRec = {
            static_cast<uint64_t>(_strPool.GetID(mod->Name)),
            static_cast<uint64_t>(mod->Access)
        };
        w.EmitRecord(ModInfo, modRec);

        for (auto &[name, v] : mod->Vars) {
            llvm::SmallVector<uint64_t, 8> varRec = {
                static_cast<uint64_t>(_strPool.GetID(v.Name.Name)),
                static_cast<uint64_t>(_typesPool.GetID(v.Type)),
                static_cast<uint64_t>(v.IsConst),
                static_cast<uint64_t>(v.Access),
                static_cast<uint64_t>(v.Storage)
            };
            w.EmitRecord(SymVar, varRec);
        }

        for (auto &[name, sub] : mod->Submods) {
            serializeModule(w, sub);
        }

        w.ExitBlock();
    }
};

}