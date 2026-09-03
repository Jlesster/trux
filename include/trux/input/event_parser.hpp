/// @file event_parser.hpp
/// @brief EventParser: an incremental, byte-at-a-time decoder that
///        turns raw terminal input (escape sequences, UTF-8, bracketed
///        paste) into Event values.

#pragma once

#include "trux/input/event.hpp"

#include <optional>
#include <vector>

namespace trux::input {

/// Stateful parser for the raw byte stream read from a terminal.
/// Feed it one byte at a time via parse(); it accumulates state
/// across calls (escape sequences, multi-byte UTF-8 sequences,
/// bracketed paste) and returns a completed Event once one is
/// recognized.
class EventParser {
public:
    /// Feeds one input byte to the parser. Returns a completed Event
    /// if `byte` finished one (e.g. a plain key, or the final byte of
    /// an escape sequence), or std::nullopt if more bytes are needed
    /// or the byte was consumed as part of an in-progress sequence.
    [[nodiscard]]
    std::optional<Event> parse(char byte);
    /// Returns and clears a byte that parse() determined did not
    /// belong to the sequence it was accumulating (e.g. a bare Escape
    /// followed by a non-CSI byte) and must be re-fed to parse() by
    /// the caller.
    [[nodiscard]]
    std::optional<char> take_reprocess();
    /// Forces resolution of an in-progress sequence that will not
    /// receive further bytes (e.g. a lone Escape with no follow-up
    /// before a timeout), returning the Event it resolves to, if any.
    [[nodiscard]]
    std::optional<Event> resolve_pending();
    /// Whether the parser is mid-sequence (has consumed bytes but not
    /// yet completed or errored). Callers can use this to decide
    /// whether to wait for more input or call resolve_pending().
    [[nodiscard]]
    bool pending() const noexcept {
        return m_state != State::Normal;
    };

private:
    /// Which kind of byte sequence is currently being accumulated.
    enum class State {
        Normal,
        Escape,
        Paste,
        CSI,
        SS3,
        UTF8,
    };

    /// One numeric parameter of a CSI sequence, with an optional
    /// colon-separated sub-parameter (as used by SGR mouse reporting).
    struct CsiParam {
        int value{-1};
        int sub{-1};
    };

    State                 m_state{State::Normal};
    std::optional<char>   m_reprocess;
    std::string           m_paste_buffer;
    std::string           m_utf8_buffer;
    std::size_t           m_paste_match{0};
    std::vector<CsiParam> m_params;
    CsiParam              m_current;
    int                   m_utf8_remaining{0};
    bool                  m_in_sub{false};
    bool                  m_sgr_mouse{false};

    /// Clears CSI parameter accumulation state and returns to Normal.
    void reset_csi();
    /// Appends a digit byte to the CSI parameter currently being read.
    void push_digit(char byte);

    /// Interprets a completed CSI sequence (params plus `final_byte`)
    /// as an Event, e.g. a special key or SGR mouse report.
    [[nodiscard]]
    std::optional<Event> finish_csi(char final_byte);
    /// Interprets a completed SGR mouse CSI sequence as a Mouse Event.
    [[nodiscard]]
    std::optional<Event> finish_sgr_mouse(char final_byte);
    /// The value of CSI parameter `i`, or `fallback` if fewer than
    /// `i + 1` parameters were given.
    [[nodiscard]]
    int param(std::size_t i, int fallback) const noexcept;
    /// The sub-parameter of CSI parameter `i`, or `fallback` if absent.
    [[nodiscard]]
    int param_sub(std::size_t i, int fallback) const noexcept;
};
}  // namespace trux::input
