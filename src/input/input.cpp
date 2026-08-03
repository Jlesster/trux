#include "trux/input/event.hpp"
#include "trux/terminal/terminal.hpp"

#include <optional>
#include <trux/input/input.hpp>

void trux::input::Input::push(char byte) {
    if(auto event = m_parser.parse(byte)) m_events.push(*event);
    if(auto reprocess = m_parser.take_reprocess()) push(*reprocess);
}

bool trux::input::Input::available() const noexcept {
    return !m_events.empty();
}

trux::input::Event trux::input::Input::poll() {
    if(m_events.empty()) return Event{};

    auto event = m_events.front();
    m_events.pop();
    return event;
}
trux::input::Event trux::input::Input::poll(Terminal& terminal) {
    if(!m_events.empty()) {
        auto event = m_events.front();
        m_events.pop();
        return event;
    }

    while(!terminal.should_quit()) {
        auto byte = terminal.read();
        if(!byte) continue;
        push(*byte);
        while(m_parser.pending() && terminal.has_pending()) {
            auto next = terminal.read();
            if(!next) break;
            push(*next);
        }

        if(m_parser.pending()) {
            if(auto event = m_parser.resolve_pending()) m_events.push(*event);
        }

        if(!m_events.empty()) {
            auto event = m_events.front();
            m_events.pop();
            return event;
        }
    }

    return Event{};
}
