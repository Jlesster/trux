#include "trux/input/event.hpp"

#include <optional>
#include <trux/input/input.hpp>

void trux::input::Input::push(char byte) {
    if(auto event = m_parser.parse(byte)) { m_events.push(*event); }
}

bool trux::input::Input::available() const noexcept {
    return !m_events.empty();
}

std::optional<trux::input::Event> trux::input::Input::poll() {
    if(m_events.empty()) return std::nullopt;

    auto event = m_events.front();
    m_events.pop();
    return event;
}
