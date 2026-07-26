#pragma once

#include <cstdint>

// [NOTE] Will be made a bitmask
enum class Style : uint8_t {
    None      = 0,
    Bold      = 1 << 0,
    Italic    = 1 << 1,
    Underline = 1 << 2,
    Blink     = 1 << 3,
    Reverse   = 1 << 4,
    Dim       = 1 << 5,
    Strike    = 1 << 6,
};
