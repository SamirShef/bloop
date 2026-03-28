#pragma once
#include <cstdint>

namespace bloop {

enum DiagLevel : uint8_t {
    Error,
    Warning,
    Note,
    Help
};

}