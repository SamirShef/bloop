#include <utils/types/types.h>

namespace bloop {

#define AS(n, t) t *Type::n() { return llvm::dyn_cast<t>(this); }

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

}