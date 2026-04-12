#include <utils/types/types.h>

namespace bloop {

#define AS(n, t) t *Type::n() { return llvm::dyn_cast<t>(this); } \
                 const t *Type::n() const { return llvm::dyn_cast<const t>(this); }

AS(AsChar, CharType)
AS(AsInteger, IntegerType)
AS(AsFloating, FloatingType)
AS(AsTuple, TupleType)
AS(AsString, StringType)
AS(AsPointer, PointerType)
AS(AsArray, ArrayType)
AS(AsStructPtr, StructType)
AS(AsTraitPtr, TraitType)
AS(AsModulePtr, ModuleType)
AS(AsUnknownNamedType, UnknownNamedType)

#undef AS

bool
operator==(const Type &lhs, const Type &rhs) {
    if (&lhs == &rhs) {
        return true;
    }
    if (lhs.GetKind() != rhs.GetKind()) {
        return false;
    }

    switch (lhs.GetKind()) {
        case Type::Integer: {
            auto l = lhs.AsInteger();
            auto r = rhs.AsInteger();
            return l->GetBitWidth() == r->GetBitWidth() && l->IsUnsigned() == r->IsUnsigned();
        }
        case Type::Pointer: {
            return *lhs.AsPointer()->GetBaseType() == *rhs.AsPointer()->GetBaseType();
        }
        case Type::Tuple: {
            auto l = lhs.AsTuple();
            auto r = rhs.AsTuple();
            if (l->GetTypesCount() != r->GetTypesCount()) {
                return false;
            }
            for (int i = 0; i < l->GetTypesCount(); ++i) {
                if (!(*l->GetTypes()[i] == *r->GetTypes()[i])) {
                    return false;
                }
            }
            return true;
        }
        case Type::StructPtr: {
            auto l = lhs.AsStructPtr();
            auto r = rhs.AsStructPtr();
            return l->GetName().Name == r->GetName().Name && *l->GetBaseMod() == *r->GetBaseMod();
        }
        default:
            return true;
    }
}

}