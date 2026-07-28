#pragma once

#include "trux/input/event_parser.hpp"

#include <optional>
#include <queue>
namespace trux::input {

class Input {
public:
    void push(char byte);

    [[nodiscard]]
    bool available() const noexcept;

    [[nodiscard]]
    std::optional<Event> poll();

private:
    EventParser       m_parser;
    std::queue<Event> m_events;
};
}  // namespace trux::input
