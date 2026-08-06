#pragma once

#include "trux/input/event_parser.hpp"

#include <queue>

namespace trux {
class Terminal;
}

namespace trux::input {

class Input {
public:
    void push(char byte);

    [[nodiscard]]
    bool available() const noexcept;
    [[nodiscard]]
    Event poll();
    [[nodiscard]]
    Event poll(Terminal&, std::span<const int> extra_fds = {});

private:
    [[nodiscard]]
    std::optional<Event> next_unconsumed(Terminal&);

    EventParser       m_parser;
    std::queue<Event> m_events;
};
}  // namespace trux::input
