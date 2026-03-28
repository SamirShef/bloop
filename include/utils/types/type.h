#pragma once
#include <cstdint>

namespace bloop {

class Type {
public:
    enum Kind : uint8_t {
        Char,
        Integer,
        Floating,
        Tuple,
        String,
        Pointer,
        Array,
        StructPtr,
        TraitPtr,
        ModulePtr
    };

private:
    Kind _kind;

public:
    explicit Type(Kind k) : _kind(k) {}

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
};

}