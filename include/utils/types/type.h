#pragma once
#include <cstdint>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SMLoc.h>
#include <string>

namespace bloop {

class Type {
public:
    enum Kind : uint8_t {
        Unknown,
        Char,
        Integer,
        Size,
        Floating,
        Tuple,
        String,
        Pointer,
        Array,
        StructPtr,
        TraitPtr,
        ModulePtr,
        Noth
    };

private:
    Kind _kind;
    llvm::SMLoc _start;
    llvm::SMLoc _end;

public:
    explicit Type(Type::Kind k, llvm::SMLoc s, llvm::SMLoc e) : _kind(k), _start(s), _end(e) {}

    Kind
    GetKind() const {
        return _kind;
    }
    
    llvm::SMLoc
    GetStartLoc() const {
        return _start;
    }

    llvm::SMLoc
    GetEndLoc() const {
        return _end;
    }
    
    constexpr bool
    Is(Kind k) const {
        return _kind == k;
    }

    constexpr bool
    IsChar() const {
        return Is(Char);
    }

    constexpr bool
    IsInteger() const {
        return Is(Integer);
    }

    constexpr bool
    IsFloating() const {
        return Is(Floating);
    }

    constexpr bool
    IsTuple() const {
        return Is(Tuple);
    }

    constexpr bool
    IsString() const {
        return Is(String);
    }

    constexpr bool
    IsPointer() const {
        return Is(Pointer);
    }

    constexpr bool
    IsArray() const {
        return Is(Array);
    }

    constexpr bool
    IsStructPtr() const {
        return Is(StructPtr);
    }

    constexpr bool
    IsTraitPtr() const {
        return Is(TraitPtr);
    }

    constexpr bool
    IsModulePtr() const {
        return Is(ModulePtr);
    }

    virtual std::string
    ToString() = 0;
};

}