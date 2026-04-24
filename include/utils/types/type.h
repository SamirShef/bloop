#pragma once
#include <cstdint>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SMLoc.h>
#include <string>

namespace bloop {

#define CLASSOF(k) constexpr static bool classof(const Type *type) { \
        return type->Is(Type::k); \
    }

class CharType;
class IntegerType;
class FloatingType;
class TupleType;
class StringType;
class PointerType;
class SliceType;
class ArrayType;
class StructType;
class TraitType;
class ModuleType;
class UnknownNamedType;
class NothType;
class SizeType;

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
        Slice,
        Struct,
        Trait,
        ModulePtr,
        Noth
    };

private:
    Kind _kind;
    llvm::SMLoc _start;
    llvm::SMLoc _end;

public:
    explicit Type(Type::Kind k, llvm::SMLoc s, llvm::SMLoc e) : _kind(k), _start(s), _end(e) {}

    #define IS(n, k) constexpr bool n() const { return Is(k); }
    #define AS(n, t) t *n(); \
                     const t *n() const;
    
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
    IsNumber() const {
        return Is(Integer) || Is(Floating) || Is(Size);
    }

    IS(IsChar, Char)
    AS(AsChar, CharType)

    IS(IsInteger, Integer)
    AS(AsInteger, IntegerType)

    IS(IsFloating, Floating)
    AS(AsFloating, FloatingType)

    IS(IsTuple, Tuple)
    AS(AsTuple, TupleType)

    IS(IsString, String)
    AS(AsString, StringType)

    IS(IsPointer, Pointer)
    AS(AsPointer, PointerType)

    IS(IsSlice, Slice)
    AS(AsSlice, SliceType)

    IS(IsArray, Array)
    AS(AsArray, ArrayType)

    IS(IsStruct, Struct)
    AS(AsStruct, StructType)

    IS(IsTrait, Trait)
    AS(AsTrait, TraitType)

    IS(IsModulePtr, ModulePtr)
    AS(AsModulePtr, ModuleType)

    IS(IsUnknownNamedType, Unknown)
    AS(AsUnknownNamedType, UnknownNamedType)

    IS(IsNothType, Noth)
    AS(AsNothType, NothType)

    IS(IsSizeType, Size)
    AS(AsSizeType, SizeType)

    #undef AS
    #undef IS

    virtual std::string
    ToString() = 0;
};

bool
operator==(const Type &lhs, const Type &rhs);

inline bool
operator!=(const Type &lhs, const Type &rhs) {
    return !(lhs == rhs);
}

}