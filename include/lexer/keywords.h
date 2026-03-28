#pragma once
#include <lexer/token.h>
#include <unordered_map>

namespace bloop {

static std::unordered_map<std::string, TokenKind> keywords {
    { "false",      TkBoolLit    },
    { "true",       TkBoolLit    },
    { "var",        TkVar        },
    { "const",      TkConst      },
    { "bool",       TkBool       },
    { "char",       TkChar       },
    { "i8",         TkI8         },
    { "u8",         TkU8         },
    { "i16",        TkI16        },
    { "u16",        TkU16        },
    { "i32",        TkI32        },
    { "u32",        TkU32        },
    { "i64",        TkI64        },
    { "u64",        TkU64        },
    { "i128",       TkI128       },
    { "u128",       TkU128       },
    { "isize",      TkISize      },
    { "usize",      TkUSize      },
    { "f32",        TkF32        },
    { "f64",        TkF64        },
    { "string",     TkString     },
    { "noth",       TkNoth       },
    { "func",       TkFunc       },
    { "return",     TkRet        },
    { "if",         TkIf         },
    { "else",       TkElse       },
    { "for",        TkFor        },
    { "break",      TkBreak      },
    { "continue",   TkContinue   },
    { "struct",     TkStruct     },
    { "pub",        TkPub        },
    { "impl",       TkImpl       },
    { "trait",      TkTrait      },
    { "nil",        TkNil        },
    { "new",        TkNew        },
    { "del",        TkDel        },
    { "mod",        TkMod        },
    { "using",      TkUsing      },
    { "static",     TkStatic     },
};

}