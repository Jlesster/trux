#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/input/modifiers.hpp"
#include "trux/input/mouse.hpp"

#include <algorithm>
#include <execution>
#include <optional>
#include <trux/input/event_parser.hpp>

using namespace trux;

void input::EventParser::reset_csi() {
    m_params.clear();
    m_current   = CsiParam{};
    m_in_sub    = false;
    m_sgr_mouse = false;
}

void input::EventParser::push_digit(char byte) {
    int  digit = byte - '0';
    int& slot  = m_in_sub ? m_current.sub : m_current.value;
    slot       = (slot < 0 ? 0 : slot * 10) + digit;
}

int input::EventParser::param(std::size_t i, int fallback) const noexcept {
    if(i >= m_params.size() || m_params[i].value < 0) return fallback;
    return m_params[i].value;
}

std::optional<input::Event> input::EventParser::parse(char byte) {
    switch(m_state) {
        case State::Normal: {
            if(byte == '\x1b') {
                m_state = State::Escape;
                return std::nullopt;
            }

            if(byte == '\n') { return Event::key(Key::Enter); }
            if(byte == '\t') { return Event::key(Key::Tab); }
            if(byte == '\x7f') { return Event::key(Key::Backspace); }
            if(byte >= 32 && byte <= 126) {
                return Event::key(static_cast<char32_t>(byte));
            }

            return std::nullopt;
        }

        case State::Escape: {
            m_state = State::Normal;
            if(byte == '[') {
                m_state = State::CSI;
                reset_csi();
                return std::nullopt;
            }
            if(byte == 'O') {
                m_state = State::SS3;
                return std::nullopt;
            }

            m_reprocess = byte;
            return Event::key(Key::Escape);
        }

        case State::SS3: {
            m_state = State::Normal;
            switch(byte) {
                case 'P':
                    return Event::key(Key::F1);
                    break;
                case 'Q':
                    return Event::key(Key::F2);
                    break;
                case 'R':
                    return Event::key(Key::F3);
                    break;
                case 'S':
                    return Event::key(Key::F4);
                    break;
                default:
                    return std::nullopt;
                    break;
            }
        }

        case State::CSI: {
            if(byte == '<' && m_params.empty() && m_current.value < 0 &&
               !m_in_sub) {
                m_sgr_mouse = true;
                return std::nullopt;
            }
            if(byte >= '0' && byte <= '9') {
                push_digit(byte);
                return std::nullopt;
            }
            if(byte == ':') {
                m_in_sub = true;
                return std::nullopt;
            }
            if(byte == ';') {
                m_params.push_back(m_current);
                m_current = CsiParam{};
                m_in_sub  = false;
                return std::nullopt;
            }

            m_params.push_back(m_current);
            m_state = State::Normal;
            if(byte == '~' && !m_sgr_mouse && param(0, 0) == 200) {
                m_state = State::Paste;
                m_paste_buffer.clear();
                m_paste_match = 0;
                return std::nullopt;
            }
            return m_sgr_mouse ? finish_sgr_mouse(byte) : finish_csi(byte);
        }

        case State::Paste: {
            static constexpr std::string_view end_seq = "\x1b[201~";

            if(byte == end_seq[m_paste_match]) {
                m_paste_match++;
                if(m_paste_match == end_seq.size()) {
                    m_state   = State::Normal;
                    auto text = std::move(m_paste_buffer);
                    m_paste_buffer.clear();
                    m_paste_match = 0;
                    return Event::from_paste(std::move(text));
                }
                return std::nullopt;
            }
            if(m_paste_match > 0) {
                m_paste_buffer.append(end_seq.substr(0, m_paste_match));
                m_paste_match = 0;
                if(byte == end_seq[0]) {
                    m_paste_match = 1;
                    return std::nullopt;
                }
            }
            m_paste_buffer.push_back(byte);
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<input::Event> input::EventParser::finish_csi(char final_byte) {
    Modifiers mods = decode_modifier_param(param(1, 1));

    auto with_mods = [&](char32_t code) { return Event::key(code, mods); };

    switch(final_byte) {
        case 'A':
            return with_mods(Key::Up);
            break;
        case 'B':
            return with_mods(Key::Down);
            break;
        case 'C':
            return with_mods(Key::Right);
            break;
        case 'D':
            return with_mods(Key::Left);
            break;
        case 'H':
            return with_mods(Key::Home);
            break;
        case 'F':
            return with_mods(Key::End);
            break;

        case '~': {
            switch(param(0, 0)) {
                case 2:
                    return with_mods(Key::Insert);
                case 3:
                    return with_mods(Key::Delete);
                case 4:
                    return with_mods(Key::End);
                case 5:
                    return with_mods(Key::PageUp);
                case 6:
                    return with_mods(Key::PageDown);
                case 7:
                    return with_mods(Key::Home);
                case 8:
                    return with_mods(Key::End);
                case 11:
                    return with_mods(Key::F1);
                case 12:
                    return with_mods(Key::F2);
                case 13:
                    return with_mods(Key::F3);
                case 14:
                    return with_mods(Key::F4);
                case 15:
                    return with_mods(Key::F5);
                case 17:
                    return with_mods(Key::F6);
                case 18:
                    return with_mods(Key::F7);
                case 19:
                    return with_mods(Key::F8);
                case 20:
                    return with_mods(Key::F9);
                case 21:
                    return with_mods(Key::F10);
                case 23:
                    return with_mods(Key::F11);
                case 24:
                    return with_mods(Key::F12);
                default:
                    return std::nullopt;
            }
        }

        case 'u': {
            int cp = param(0, -1);
            if(cp < 0) return std::nullopt;

            switch(cp) {
                case 9:
                    return with_mods(Key::Tab);
                case 13:
                    return with_mods(Key::Enter);
                case 27:
                    return with_mods(Key::Escape);
                case 127:
                    return with_mods(Key::Backspace);
                default:
                    return with_mods(static_cast<char32_t>(cp));
            }
        }
        default:
            return std::nullopt;
            break;
    }
}

std::optional<input::Event>
input::EventParser::finish_sgr_mouse(char final_byte) {
    if(final_byte != 'M' && final_byte != 'm') return std::nullopt;

    int cb = param(0, -1);
    int cx = param(1, -1);
    int cy = param(2, -1);
    if(cb < 0 || cx < 0 || cy < 0) return std::nullopt;

    Modifiers mods{};
    if(cb & 4) mods.add(Mod::Shift);
    if(cb & 8) mods.add(Mod::Alt);
    if(cb & 16) mods.add(Mod::Ctrl);

    bool is_wheel  = cb & 64;
    bool is_motion = cb & 32;
    bool btn_bits  = cb & 0x3;

    MouseEvent m{};
    m.position = {cx - 1, cy - 1};
    m.mods     = mods;

    if(is_wheel) {
        m.kind = (btn_bits == 0) ? MouseKind::ScrollUp : MouseKind::ScrollDown;
        m.button = MouseButton::None;
    } else {
        m.button = (btn_bits == 0)   ? MouseButton::Left
                   : (btn_bits == 1) ? MouseButton::Middle
                   : (btn_bits == 1) ? MouseButton::Right
                                     : MouseButton::None;

        m.kind = is_motion             ? MouseKind::Drag
                 : (final_byte == 'M') ? MouseKind::Press
                                       : MouseKind::Release;
    }
    return Event::from_mouse(m);
}

std::optional<input::Event> input::EventParser::resolve_pending() {
    bool was_escape = (m_state == State::Escape || m_state == State::SS3);
    m_state         = State::Normal;
    if(was_escape) return Event::key(Key::Escape);
    return std::nullopt;
}

std::optional<char> input::EventParser::take_reprocess() {
    auto b = m_reprocess;
    m_reprocess.reset();
    return b;
}
