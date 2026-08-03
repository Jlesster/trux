#include "trux/input/event.hpp"
#include "trux/input/key.hpp"

#include <optional>
#include <trux/input/event_parser.hpp>

using namespace trux;

std::optional<input::Event> input::EventParser::parse(char byte) {
    switch(m_state) {
        case State::Normal: {
            if(byte == '\x1b') {
                m_state = State::Escape;
                return std::nullopt;
            }

            if(byte == '\n') { return Event{Key::Enter, true}; }
            if(byte == '\t') { return Event{Key::Tab, true}; }
            if(byte == '\x7f') { return Event{Key::Backspace, true}; }
            if(byte >= 32 && byte <= 126) {
                return Event{static_cast<char32_t>(byte), true};
            }

            return std::nullopt;
        }

        case State::Escape: {
            m_state = State::Normal;
            if(byte == '[') {
                m_state = State::CSI;
                return std::nullopt;
            }

            m_reprocess = byte;
            return Event{Key::Escape, true};
        }

        case State::CSI: {
            m_state = State::Normal;

            switch(byte) {
                case 'A':
                    return Event{Key::Up, true};
                case 'B':
                    return Event{Key::Down, true};
                case 'C':
                    return Event{Key::Right, true};
                case 'D':
                    return Event{Key::Left, true};
            }
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<input::Event> input::EventParser::resolve_pending() {
    bool was_escape = (m_state == State::Escape);
    m_state         = State::Normal;
    if(was_escape) return Event{Key::Escape, true};
    return std::nullopt;
}

std::optional<char> input::EventParser::take_reprocess() {
    auto b = m_reprocess;
    m_reprocess.reset();
    return b;
}
