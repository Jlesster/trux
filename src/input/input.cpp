#include "trux/input/event.hpp"
#include "trux/terminal/terminal.hpp"

#include <optional>
#include <trux/input/event.hpp>
#include <trux/input/input.hpp>

using namespace trux;

void input::Input::push(char byte) {
    if(auto event = m_parser.parse(byte)) m_events.push(*event);
    if(auto reprocess = m_parser.take_reprocess()) push(*reprocess);
}

bool input::Input::available() const noexcept { return !m_events.empty(); }

input::Event input::Input::poll() {
    if(m_events.empty()) return Event{};

    auto event = m_events.front();
    m_events.pop();
    return event;
}
input::Event input::Input::poll(Terminal& terminal) {
    if(auto event = next_unconsumed(terminal)) return *event;

    while(!terminal.should_quit()) {
        auto byte = terminal.read();
        if(!byte) continue;
        push(*byte);
        while(m_parser.pending() && terminal.has_pending(25)) {
            auto next = terminal.read();
            if(!next) break;
            push(*next);
        }

        if(m_parser.pending()) {
            if(auto event = m_parser.resolve_pending()) m_events.push(*event);
        }
        if(auto event = next_unconsumed(terminal)) return *event;
    }
    return Event{};
}

std::optional<input::Event> input::Input::next_unconsumed(Terminal& terminal) {
    if(m_events.empty()) return std::nullopt;
    auto event = m_events.front();
    m_events.pop();
    (void)terminal.dispatch(event);
    return event;
}
