#pragma once

#include "trux/input/event.hpp"

#include <optional>
#include <vector>

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
        SS3,
    };

    struct CsiParam {
        int value{-1};
        int sub{-1};
    };

    State               m_state{State::Normal};
    std::optional<char> m_reprocess;

    std::vector<CsiParam> m_params;
    CsiParam              m_current;
    bool                  m_in_sub{false};
    bool                  m_sgr_mouse{false};

    void reset_csi();
    void push_digit(char byte);

    [[nodiscard]]
    std::optional<Event> finish_csi(char final_byte);
    [[nodiscard]]
    std::optional<Event> finish_sgr_mouse(char final_byte);
    [[nodiscard]]
    int param(std::size_t i, int fallback) const noexcept;
};
}  // namespace trux::input
