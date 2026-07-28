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

            if(byte == '\n') { return Event{KeyEvent{.key = Key::Enter}}; }

            if(byte == '\t') { return Event{KeyEvent{.key = Key::Tab}}; }

            if(byte == '\x7f') {
                return Event{KeyEvent{.key = Key::Backspace}};
            }

            if(byte >= 32 && byte <= 126) {
                return Event{
                    KeyEvent{.key       = Key::Char,
                             .character = static_cast<char32_t>(byte)}
                };
            }

            return std::nullopt;
        }

        case State::Escape: {
            if(byte == '[') {
                m_state = State::CSI;
                return std::nullopt;
            }

            m_state = State::Normal;

            return Event{KeyEvent{.key = Key::Escape}};
        }

        case State::CSI: {
            m_state = State::Normal;

            switch(byte) {
                case 'A':
                    return Event{{KeyEvent{Key::Up}}};
                case 'B':
                    return Event{{KeyEvent{Key::Down}}};
                case 'C':
                    return Event{{KeyEvent{Key::Right}}};
                case 'D':
                    return Event{{KeyEvent{Key::Left}}};
            }
            return std::nullopt;
        }
    }

    return std::nullopt;
}
