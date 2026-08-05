#pragma once

#include <optional>
#include <string_view>
#include <utility>

namespace trux::util {
[[nodiscard]]
inline std::optional<std::pair<char32_t, int>> decode_utf8(std::string_view s) {
    if(s.empty()) return std::nullopt;
    unsigned char b0 = s[0];

    if(b0 < 0x80) return std::pair{char32_t{b0}, 1};
    int len = (b0 & 0xE0) == 0xC0   ? 2
              : (b0 & 0xF0) == 0xE0 ? 3
              : (b0 & 0xF8) == 0xF0 ? 4
                                    : 0;

    if(len == 0 || s.size() < static_cast<size_t>(len)) return std::nullopt;

    char32_t cp = b0 & (0xFF >> (len + 1));
    for(int i = 1; i < len; i++)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
    return std::pair{cp, len};
}

[[nodiscard]]
int glyph_width(char32_t cp) noexcept;
}  // namespace trux::util
