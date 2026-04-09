#pragma once
#include <cstdint>

namespace bloop {

enum DiagLevel : uint8_t {
    Error,
    Warning,
    Note,
    Help
};

enum ErrorCode : uint8_t {
    ErrEmptyCharLit = 1,
    ErrLongCharLit,
    ErrInvalidEscapeSequence,
    ErrExpectedSemi,
    ErrExpectedId,
    ErrExpectedType,
    ErrExpectedToken,
    ErrExpectedStmt,
    ErrExpectedExpr,
    ErrUnknownType,
    ErrCannotInferenceType,
    ErrCannotFindCommonType,
    ErrCannotApplyOp,
    ErrCannotGetDefault,
    ErrUndeclaredVar,
    ErrUndeclaredFunc,
    ErrRedefinition,
    ErrCannotImplCast,
    ErrHasntRet,
    ErrModNotFound,
    ErrModNotLoaded,
    ErrUnimplementedExpr,
    ErrUnimplementedStmt,
    ErrNoMatchingFunction,
    ErrAmbiguousCall,
    ErrInvalidMainFuncSig,
    ErrPrivateSymbol,
    ErrUndeclaredSymbol,
    ErrReasgnConst,
};

enum WarnCode : uint8_t {
    WarnLostPrecision,
    WarnLiteralUnderflow,
};

}