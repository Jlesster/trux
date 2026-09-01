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
    void set_want_tick(bool want) noexcept { m_want_tick = want; }

    [[nodiscard]]
    bool available() const noexcept;
    [[nodiscard]]
    Event poll();
    [[nodiscard]]
    Event poll(Terminal&, std::span<const int> extra_fds = {});

private:
    [[nodiscard]]
    std::optional<Event> next_unconsumed(Terminal&);

    bool              m_want_tick = false;
    EventParser       m_parser;
    std::queue<Event> m_events;
};
}  // namespace trux::input
