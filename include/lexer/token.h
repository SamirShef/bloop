#pragma once
#include <cstdint>
#include <llvm/Support/SMLoc.h>
#include <string>

namespace bloop {

enum TokenKind : uint8_t {
    TkId,               // identifier
    TkVar,              // keyword `var`
    TkConst,            // keyword `const`
    TkChar,             // keyword `char`
    TkBool,             // keyword `bool`
    TkI8,               // keyword `i8`
    TkI16,              // keyword `i16`
    TkI32,              // keyword `i32`
    TkI64,              // keyword `i64`
    TkI128,             // keyword `i128`
    TkISize,            // keyword `isize`
    TkU8,               // keyword `u8`
    TkU16,              // keyword `u16`
    TkU32,              // keyword `u32`
    TkU64,              // keyword `u64`
    TkU128,             // keyword `u128`
    TkUSize,            // keyword `usize`
    TkF32,              // keyword `f32`
    TkF64,              // keyword `f64`
    TkString,           // keyword `string`
    TkNoth,             // keyword `noth`
    TkFunc,             // keyword `func`
    TkRet,              // keyword `return`
    TkIf,               // keyword `if`
    TkElse,             // keyword `else`
    TkFor,              // keyword `for`
    TkBreak,            // keyword `break`
    TkContinue,         // keyword `continue`
    TkStruct,           // keyword `struct`
    TkPub,              // keyword `pub`
    TkImpl,             // keyword `impl`
    TkTrait,            // keyword `trait`
    TkNil,              // keyword `nil`
    TkNew,              // keyword `new`
    TkDel,              // keyword `del`
    TkMod,              // keyword `mod`
    TkUsing,            // keyword `using`
    TkStatic,           // keyword `static`
    
    TkBoolLit,          // boolean literal
    TkCharLit,          // character literal
    TkI16Lit,           // i16 literal
    TkI32Lit,           // i32 literal
    TkI64Lit,           // i64 literal
    TkI128Lit,          // i128 literal
    TkISizeLit,         // isize literal
    TkU16Lit,           // u16 literal
    TkU32Lit,           // u32 literal
    TkU64Lit,           // u64 literal
    TkU128Lit,          // u128 literal
    TkUSizeLit,         // usize literal
    TkF32Lit,           // f32 literal
    TkF64Lit,           // f64 literal
    TkStrLit,           // string literal
    
    TkSemi,             // `;`
    TkComma,            // `,`
    TkDot,              // `.`
    TkLParen,           // `(`
    TkRParen,           // `)`
    TkLBrace,           // `{`
    TkRBrace,           // `}`
    TkLBracket,         // `[`
    TkRBracket,         // `]`
    TkTilde,            // `~`
    TkQuestion,         // `?`
    TkColon,            // `:`
    TkEq,               // `=`
    TkBang,             // `!`
    TkAnd,              // `&`
    TkOr,               // `|`
    TkAndEq,            // `&=`
    TkOrEq,             // `|=`
    TkLogAnd,           // `&&`
    TkLogOr,            // `||`
    TkPlus,             // `+`
    TkMinus,            // `-`
    TkStar,             // `*`
    TkSlash,            // `/`
    TkPercent,          // `%`
    TkLtLt,             // `<<`
    TkGtGt,             // `>>`
    TkPlusEq,           // `+=`
    TkMinusEq,          // `-=`
    TkStarEq,           // `*=`
    TkSlashEq,          // `/=`
    TkPercentEq,        // `%=`
    TkLtLtEq,           // `<<=`
    TkGtGtEq,           // `>>=`
    TkEqEq,             // `==`
    TkNotEq,            // `!=`
    TkLt,               // `<`
    TkGt,               // `>`
    TkLtEq,             // `<=`
    TkGtEq,             // `>=`
    TkCarret,           // `^`

    TkEof,
    TkUnknown,          // Unknown token, not expected by the lexer
};

struct Token {
    TokenKind Kind;
    std::string Val;
    
    llvm::SMLoc Start;
    llvm::SMLoc End;

    explicit Token(TokenKind k, std::string v, llvm::SMLoc s, llvm::SMLoc e) : Kind(k), Val(v), Start(s), End(e) {}
};

}