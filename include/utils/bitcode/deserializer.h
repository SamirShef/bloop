#pragma once
#include <utils/bitcode/serializer.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <utils/modules/module.h>

namespace bloop {

class Deserializer {
    std::vector<std::string> _strPool;
    std::vector<Type *> _parsedTypes;

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
        }
    }

    void
    deserializeVar(Module *mod, llvm::SmallVector<uint64_t, 64> &record) {
        llvm::SMLoc emptyLoc;
        
        NameObj name(_strPool[record[0]], emptyLoc, emptyLoc);
        Type *type = _parsedTypes[record[1]];
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
        Type *retType = _parsedTypes[record[1]];
        AccessModifier access = static_cast<AccessModifier>(record[2]);
        StorageKind storage = static_cast<StorageKind>(record[3]);
        int argsCount = static_cast<int>(record[4]);
        std::vector<Argument> args;
        for (int i = 0; i < argsCount; ++i) {
            NameObj argName("", emptyLoc, emptyLoc);
            Type *argType = _parsedTypes[record[5 + i]];
            args.push_back(Argument(argName, argType));
        }

        Function func(name, retType, args, access, storage, mod);
        mod->FuncOverloads[name.Name].Candidates.push_back(func);
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
                        subMod = new Module(subName, static_cast<AccessModifier>(record[1]));
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
    readTypePool(llvm::BitstreamCursor &cursor) {
        cursor.EnterSubBlock(TypePoolBlockID);
        _parsedTypes.push_back(nullptr);

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
                    _parsedTypes.push_back(new IntegerType(record[0], record[1], emptyLoc, emptyLoc));
                    break;
                case TypeIdFloating:
                    _parsedTypes.push_back(new FloatingType(static_cast<FloatingType::FloatingKind>(record[0]), emptyLoc, emptyLoc));
                    break;
                case TypeIdPointer:
                    _parsedTypes.push_back(new PointerType(_parsedTypes[record[0]], emptyLoc, emptyLoc));
                    break;
                case TypeIdStruct:
                    _parsedTypes.push_back(new StructType(NameObj(_strPool[record[0]], emptyLoc, emptyLoc), nullptr, emptyLoc, emptyLoc));
                    break;
                case TypeIdTrait:
                    _parsedTypes.push_back(new TraitType(NameObj(_strPool[record[0]], emptyLoc, emptyLoc), nullptr, emptyLoc, emptyLoc));
                    break;
                case TypeIdNoth:
                    _parsedTypes.push_back(new NothType(emptyLoc, emptyLoc));
                    break;
            }
        }
    }
};

}