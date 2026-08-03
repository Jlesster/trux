#pragma once

#include "trux/input/event.hpp"

#include <optional>
namespace trux::input {

class EventParser {
public:
    [[nodiscard]]
    std::optional<Event> parse(char byte);
    [[nodiscard]]
    std::optional<char> take_reprocess();
    [[nodiscard]]
    std::optional<Event> resolve_pending();
    [[nodiscard]]
    bool pending() const noexcept {
        return m_state != State::Normal;
    };

private:
    enum class State {
        Normal,
        Escape,
        CSI,
    };

    State               m_state{State::Normal};
    std::optional<char> m_reprocess;
};
}  // namespace trux::input
