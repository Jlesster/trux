#pragma once

#include "trux/input/key.hpp"
#include "trux/renderer/draw_command.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <trux/component/border.hpp>
#include <trux/component/component_flags.hpp>
#include <trux/input/event.hpp>
#include <trux/renderer/draw_command_buffer.hpp>
#include <trux/style/style.hpp>
#include <trux/util/util.hpp>

namespace trux::component {

struct TextInput {
    TextInput(std::string& value, int& cursor) : value(value), cursor(cursor) {}

    std::string& value;
    int&         cursor;

    ComponentFlags flags{};
    style::Style   cursor_style{style::Style::Reverse};

    mutable int m_view_start{0};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd);
            content = inset(area);
        }

        auto [w, h] = content.size();
        if(w <= 0 || h <= 0) return;

        cursor = std::clamp(cursor, 0, static_cast<int>(value.size()));

        auto col_width = [&](int from, int to) {
            int col = 0, i = from;
            while(i < to) {
                auto decoded = util::decode_utf8(
                    std::string_view(value).substr(static_cast<size_t>(i)));

                char32_t cp = decoded ? decoded->first : U'?';
                col += std::max(1, util::glyph_width(cp));
                i = util::next_boundary(value, i);
            }
            return col;
        };

        if(cursor < m_view_start) m_view_start = cursor;
        else {
            int col = col_width(m_view_start, cursor);
            if(col >= w) {
                int excess  = col - w + 1;
                int dropped = 0;
                int i       = m_view_start;
                while(dropped < excess && i < cursor) {
                    auto decoded = util::decode_utf8(
                        std::string_view(value).substr(static_cast<size_t>(i)));
                    char32_t cp = decoded ? decoded->first : U'?';
                    dropped += std::max(1, util::glyph_width(cp));
                    i = util::next_boundary(value, i);
                }
                m_view_start = i;
            }
        }

        int end = m_view_start;
        int col = 0;
        while(end < static_cast<int>(value.size())) {
            auto decoded = util::decode_utf8(
                std::string_view(value).substr(static_cast<size_t>(end)));
            char32_t cp = decoded ? decoded->first : U'?';
            int      gw = std::max(1, util::glyph_width(cp));
            if(col + gw > w) break;
            col += gw;
            end = util::next_boundary(value, end);
        }

        std::string_view visible{value.data() + m_view_start,
                                 static_cast<size_t>(end - m_view_start)};
        auto             pos        = content.position();
        int              cursor_col = col_width(m_view_start, cursor);
        if(cursor <= m_view_start) {
            cmd.push(renderer::DrawText{pos, " ", cursor_style});
            cmd.push(renderer::DrawText{
                {pos.x + 1, pos.y},
                visible
            });
        } else {
            size_t split  = static_cast<size_t>(cursor - m_view_start);
            auto   before = visible.substr(0, std::min(split, visible.size()));
            auto   after  = visible.substr(std::min(split, visible.size()));

            cmd.push(renderer::DrawText{pos, before});
            auto cursor_pos = pos;
            cursor_pos.x += cursor_col;

            if(after.empty()) {
                cmd.push(renderer::DrawText{cursor_pos, " ", cursor_style});
            } else {
                int  glyph_len = util::next_boundary(after, 0);
                auto decoded   = util::decode_utf8(after);
                int  gw        = decoded
                                     ? std::max(1, util::glyph_width(decoded->first))
                                     : 1;

                cmd.push(renderer::DrawText{
                    cursor_pos, after.substr(0, glyph_len), cursor_style});
                cmd.push(renderer::DrawText{
                    {cursor_pos.x + gw, cursor_pos.y},
                    after.substr(glyph_len)
                });
            }
        }
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(event.kind == input::EventKind::Paste) {
            std::string sanitized;
            sanitized.reserve(event.paste.size());
            for(char c : event.paste) {
                auto b = static_cast<unsigned char>(c);
                if(b == '\n' || b == '\r' || b == '\t' || b < 0x20) continue;
                sanitized.push_back(c);
            }
            value.insert(static_cast<size_t>(cursor), sanitized);
            cursor += static_cast<int>(sanitized.size());
            return true;
        }

        if(event.kind != input::EventKind::Key) return false;

        switch(event.code) {
            case input::Key::Left:
                cursor = util::prev_boundary(value, cursor);
                return true;
            case input::Key::Right:
                cursor = util::next_boundary(value, cursor);
                return true;
            case input::Key::Home:
                cursor = 0;
                return true;
            case input::Key::End:
                cursor = static_cast<int>(value.size());
                return true;
            case input::Key::Backspace: {
                if(cursor == 0) return true;
                int start = event.mods.has(input::Mod::Ctrl)
                                ? util::prev_word_boundary(value, cursor)
                                : util::prev_boundary(value, cursor);
                value.erase(static_cast<size_t>(start),
                            static_cast<size_t>(cursor - start));
                cursor = start;
                return true;
            }
            case input::Key::Delete: {
                if(cursor >= static_cast<int>(value.size())) return true;
                int end = event.mods.has(input::Mod::Ctrl)
                              ? util::next_word_boundary(value, cursor)
                              : util::next_boundary(value, cursor);
                value.erase(static_cast<size_t>(cursor),
                            static_cast<size_t>(end - cursor));
                return true;
            }
            default:
                break;
        }
        if(event.mods.has(input::Mod::Ctrl) || event.mods.has(input::Mod::Alt))
            return false;

        if(event.code >= 32 && event.code < input::KeyBackSpace) {
            auto encoded = util::encode_utf8(event.code);
            value.insert(static_cast<size_t>(cursor), encoded);
            cursor += static_cast<int>(encoded.size());
            return true;
        }
        return false;
    }
};
}  // namespace trux::component
