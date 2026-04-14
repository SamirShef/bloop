#pragma once
#include <utils/bitcode/serializer.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <utils/modules/module.h>

namespace bloop {

class Deserializer {
    std::vector<std::string> _strPool;
    std::vector<Type *> _types;
    std::vector<Module *> _modules;

public:
    bool
    DeserializeInto(Module *root, const std::string &fileName) {
        if (!root) {
            return false;
        }
        
        auto buffer = llvm::MemoryBuffer::getFile(fileName);
        if (!buffer) {
            return false;
        }

        llvm::BitstreamCursor cursor(buffer.get()->getMemBufferRef());

        for (int i = 0; i < 3; ++i) {
            cursor.Read(8).get();
        }

        while (!cursor.AtEndOfStream()) {
            auto entryOrErr = cursor.advance();
            if (!entryOrErr) {
                llvm::consumeError(entryOrErr.takeError());
                break;
            }
            auto entry = entryOrErr.get();
            if (entry.Kind != llvm::BitstreamEntry::SubBlock) {
                continue;
            }

            if (entry.ID == StrPoolBlockID) {
                cursor.EnterSubBlock(StrPoolBlockID);
                while (!cursor.AtEndOfStream()) {
                    auto e = cursor.advance().get();
                    if (e.Kind == llvm::BitstreamEntry::EndBlock) {
                        break;
                    }
                    llvm::SmallVector<uint64_t, 64> Record;
                    cursor.readRecord(e.ID, Record);
                    std::string s;
                    for (uint64_t val : Record) {
                        s += (char)val;
                    }
                    _strPool.push_back(s);
                }
            }
            else if (entry.ID == ModPoolBlockID) {
                readModPool(root, cursor);
            }
            else if (entry.ID == TypePoolBlockID) {
                readTypePool(cursor);
            }
            else if (entry.ID == ModBlockID) {
                readIntoModule(root, cursor);
            }
            else {
                cursor.SkipBlock();
            }
        }
        return true;
    }

private:
    void
    readIntoModule(Module *mod, llvm::BitstreamCursor &cursor) {
        if (llvm::Error err = cursor.EnterSubBlock(ModBlockID)) {
            return;
        }

        while (!cursor.AtEndOfStream()) {
            auto entryOrErr = cursor.advance();
            if (!entryOrErr) {
                llvm::consumeError(entryOrErr.takeError());
                break;
            }
            auto entry = entryOrErr.get();
            
            if (entry.Kind == llvm::BitstreamEntry::EndBlock) {
                break;
            }
            
            if (entry.Kind == llvm::BitstreamEntry::SubBlock) {
                if (entry.ID == ModBlockID) {
                    handleSubmodule(mod, cursor);
                }
                else {
                    cursor.SkipBlock();
                }
                continue;
            }

            llvm::SmallVector<uint64_t, 64> record;
            auto code = cursor.readRecord(entry.ID, record).get();

            if (code == ModInfo) {
                mod->Access = static_cast<AccessModifier>(record[1]);
            }
            else if (code == SymVar && mod) {
                deserializeVar(mod, record);
            }
            else if (code == SymFunc && mod) {
                deserializeFunc(mod, record);
            }
            else if (code == SymStruct && mod) {
                deserializeStruct(mod, record);
            }
        }
    }

    void
    deserializeVar(Module *mod, llvm::SmallVector<uint64_t, 64> &record) {
        llvm::SMLoc emptyLoc;
        
        NameObj name(_strPool[record[0]], emptyLoc, emptyLoc);
        Type *type = _types[record[1]];
        bool isConst = static_cast<bool>(record[2]);
        AccessModifier access = static_cast<AccessModifier>(record[3]);
        StorageKind storage = static_cast<StorageKind>(record[4]);
        int index = static_cast<int>(record[5]);

        Variable v(name, type, isConst, access, Value::GetIncorrectValue(), storage, index);
        mod->Vars[name.Name] = v;
    }

    void
    deserializeFunc(Module *mod, llvm::SmallVector<uint64_t, 64> &record) {
        llvm::SMLoc emptyLoc;
        
        NameObj name(_strPool[record[0]], emptyLoc, emptyLoc);
        Type *retType = _types[record[1]];
        AccessModifier access = static_cast<AccessModifier>(record[2]);
        StorageKind storage = static_cast<StorageKind>(record[3]);
        int argsCount = static_cast<int>(record[4]);
        std::vector<Argument> args;
        for (int i = 0; i < argsCount; ++i) {
            NameObj argName("", emptyLoc, emptyLoc);
            Type *argType = _types[record[5 + i]];
            args.push_back(Argument(argName, argType));
        }

        Function func(name, retType, args, access, storage, mod);
        mod->FuncOverloads[name.Name].Candidates.push_back(func);
    }

    void
    deserializeStruct(Module *mod, llvm::SmallVector<uint64_t, 64> &record) {
        // TODO: add logic for deserialize methods
        llvm::SMLoc emptyLoc;
        
        NameObj name(_strPool[record[0]], emptyLoc, emptyLoc);
        AccessModifier access = static_cast<AccessModifier>(record[1]);
        Module *parent = _modules[record[2]];
        int fieldsCount = record[3];
        std::vector<Field> fields;
        for (int i = 0; i < fieldsCount; ++i) {
            NameObj fName(_strPool[record[4 + i * 4]], emptyLoc, emptyLoc);
            Variable fVar(fName, _types[record[4 + i * 4 + 1]], false, static_cast<AccessModifier>(record[4 + i * 4 + 2]), Value::GetIncorrectValue());
            fields.push_back(Field(fVar, static_cast<bool>(record[4 + i * 4 + 3]), fVar.Access));
        }
        Struct s(name, parent, fields, access);
        mod->Structs.emplace(name.Name, s);
    }

    void
    handleSubmodule(Module *parent, llvm::BitstreamCursor &cursor) {
        if (llvm::Error err = cursor.EnterSubBlock(ModBlockID)) {
            return;
        }

        while (!cursor.AtEndOfStream()) {
            auto entry = cursor.advance().get();
            if (entry.Kind == llvm::BitstreamEntry::EndBlock) {
                break;
            }
            
            if (entry.Kind == llvm::BitstreamEntry::Record) {
                llvm::SmallVector<uint64_t, 64> record;
                auto code = cursor.readRecord(entry.ID, record).get();

                if (code == ModInfo) {
                    std::string subName = _strPool[record[0]];
                    
                    Module *subMod;
                    if (parent->Submods.count(subName)) {
                        subMod = parent->Submods[subName];
                    }
                    else {
                        subMod = new Module(subName, static_cast<AccessModifier>(record[1]), parent);
                        parent->Submods[subName] = subMod;
                    }

                    readIntoModule(subMod, cursor);
                    return;
                }
            }
            else {
                cursor.SkipBlock();
            }
        }
    }

    void
    readModPool(Module *root, llvm::BitstreamCursor &cursor) {
        cursor.EnterSubBlock(ModPoolBlockID);
        _modules.push_back(nullptr);

        while (!cursor.AtEndOfStream()) {
            auto entryOrErr = cursor.advance();
            if (!entryOrErr) {
                break;
            }
            auto entry = entryOrErr.get();
            
            if (entry.Kind == llvm::BitstreamEntry::EndBlock) {
                break;
            }
            if (entry.Kind != llvm::BitstreamEntry::Record) {
                continue;
            }

            llvm::SmallVector<uint64_t, 2> record;
            cursor.readRecord(entry.ID, record);

            std::string name = _strPool[record[0]];
            uint32_t parentId = record[1];

            Module *parentMod = parentId < _modules.size() ? _modules[parentId] : nullptr;
            Module *newMod = nullptr;

            if (!parentMod) {
                if (root && root->Name == name) {
                    newMod = root;
                }
                else {
                    newMod = new Module(name, Pub, nullptr);
                }
            }
            else {
                if (parentMod->Submods.count(name)) {
                    newMod = parentMod->Submods[name];
                }
                else {
                    newMod = new Module(name, Pub, parentMod);
                    parentMod->Submods[name] = newMod;
                }
            }
            
            _modules.push_back(newMod);
        }
    }

    void
    readTypePool(llvm::BitstreamCursor &cursor) {
        cursor.EnterSubBlock(TypePoolBlockID);
        _types.push_back(nullptr);

        llvm::SMLoc emptyLoc;

        while (!cursor.AtEndOfStream()) {
            auto entry = cursor.advance().get();
            if (entry.Kind == llvm::BitstreamEntry::EndBlock) {
                break;
            }

            llvm::SmallVector<uint64_t, 8> record;
            auto code = cursor.readRecord(entry.ID, record).get();

            switch (code) {
                case TypeIdInteger:
                    _types.push_back(new IntegerType(record[0], record[1], emptyLoc, emptyLoc));
                    break;
                case TypeIdFloating:
                    _types.push_back(new FloatingType(static_cast<FloatingType::FloatingKind>(record[0]), emptyLoc, emptyLoc));
                    break;
                case TypeIdPointer:
                    _types.push_back(new PointerType(_types[record[0]], emptyLoc, emptyLoc));
                    break;
                case TypeIdStruct: {
                    Module *baseMod = record[1] < _modules.size() ? _modules[record[1]] : nullptr;
                    _types.push_back(new StructType(NameObj(_strPool[record[0]], emptyLoc, emptyLoc), baseMod, emptyLoc, emptyLoc));
                    break;
                }
                case TypeIdTrait: {
                    Module *baseMod = record[1] < _modules.size() ? _modules[record[1]] : nullptr;
                    _types.push_back(new TraitType(NameObj(_strPool[record[0]], emptyLoc, emptyLoc), baseMod, emptyLoc, emptyLoc));
                    break;
                }
                case TypeIdNoth:
                    _types.push_back(new NothType(emptyLoc, emptyLoc));
                    break;
            }
        }
    }
};

}