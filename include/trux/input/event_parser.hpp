#pragma once

#include "trux/input/event.hpp"

#include <optional>
namespace trux::input {

class EventParser {
public:
    [[nodiscard]]
    std::optional<Event> parse(char byte);

private:
    enum class State {
        Normal,
        Escape,
        CSI,
    };

    State m_state{State::Normal};
};
}  // namespace trux::input
