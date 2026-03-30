#pragma once
#include <cstdint>

namespace bloop {

enum StorageKind : uint8_t {
    Static,
    Stack,
    Parameter,
    Member,
};

}