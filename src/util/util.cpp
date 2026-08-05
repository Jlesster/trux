#include <trux/util/util.hpp>

namespace trux::util {

int glyph_width(char32_t cp) noexcept {
    if(cp == 0) return 0;
    if(cp < 32 || (cp >= 0x7F && cp < 0xa0)) return 0;

    if((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
       (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF))
        return 0;

    if((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
       (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
       (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0x20000 && cp <= 0x3FFFF))
        return 2;

    if(cp >= 0x1F300 && cp <= 0x1FAFF) return 2;

    return 1;
}
}  // namespace trux::util
