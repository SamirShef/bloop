#pragma once
#include <cstdint>

namespace bloop {

enum StorageKind : uint8_t {
    Extern,
    Static,
    Stack,
    Parameter,
    Member,
};

}