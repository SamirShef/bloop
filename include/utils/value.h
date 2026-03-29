#pragma once
#include <string>
#include <utils/types/type.h>
#include <variant>

namespace bloop {

using ValueData = std::variant<int64_t, double, std::string>;

struct Value {
    enum Kind : uint8_t {
        Unknown,
        Const,
        Nil
    } Kind;
    ValueData Data;
    class Type *Type;

    llvm::SMLoc Start;
    llvm::SMLoc End;

    explicit Value(enum Kind k, ValueData d, class Type *t, llvm::SMLoc s, llvm::SMLoc e) : Kind(k), Data(d), Type(t), Start(s), End(e) {}

    void
    Delete() {
        delete Type;
    }

    std::string
    ToString() const {
        switch (Kind) {
            case Unknown:
                return "<unknown>";
            case Const:
                return toStringAsConst();
            case Nil:
                return "<nil>";
        }
    }

private:
    std::string
    toStringAsConst() const {
        switch (Type->GetKind()) {
        #define TYPE(k, s) case Type::k: return s;
            TYPE(Unknown, "<unknown>")
            TYPE(Char, std::get<2>(Data))
            TYPE(Integer, std::to_string(std::get<0>(Data)))
            TYPE(Floating, std::to_string(std::get<1>(Data)))
            TYPE(Tuple, "<tuple>")
            TYPE(String, std::get<2>(Data))
            TYPE(Pointer, Kind == Nil ? "<nil>" : "<pointer>")
            TYPE(Array, "<array>")
            TYPE(StructPtr, "<struct " + Type->ToString() + ">")
            TYPE(TraitPtr, "<trait " + Type->ToString() + ">")
            TYPE(ModulePtr, "<module " + Type->ToString() + ">")
            TYPE(Noth, "<noth>")
            default:
                return "";
        #undef TYPE
        }
    }
};

}