/// @file input.hpp
/// @brief Input: the queue and blocking-read front end sitting on top
///        of EventParser, used to actually drive an event loop.

#pragma once

#include "trux/input/event_parser.hpp"

#include <queue>

namespace trux {
class Terminal;
}

namespace trux::input {

/// Buffers and parses raw terminal bytes into a queue of Events, and
/// optionally blocks reading directly from a Terminal (plus extra
/// file descriptors, e.g. async::Channel/Executor fds) to produce
/// the next event for an event loop.
class Input {
public:
    /// Feeds one raw byte from the terminal into the parser, queuing
    /// a completed Event if one results.
    void push(char byte);
    /// Enables or disables emitting periodic Event::tick() events
    /// from poll(Terminal&, ...) while otherwise idle.
    void set_want_tick(bool want) noexcept { m_want_tick = want; }

    /// Whether an already-parsed Event is queued and ready via poll().
    [[nodiscard]]
    bool available() const noexcept;
    /// Pops and returns the next queued Event, or Event::none() if
    /// none is available. Does not block or read from a terminal.
    [[nodiscard]]
    Event poll();
    /// Blocks (via `term`'s underlying fd, plus any `extra_fds`)
    /// until an Event is available and returns it: a queued Event if
    /// one is already pending, otherwise reads and parses bytes from
    /// `term` until one completes. Returns a Tick event periodically
    /// if set_want_tick(true) was called, or an Async event if one of
    /// `extra_fds` became readable.
    [[nodiscard]]
    Event poll(Terminal&, std::span<const int> extra_fds = {});

private:
    /// Reads and parses bytes from `term` until either a non-consumed
    /// Event completes or the read would block.
    [[nodiscard]]
    std::optional<Event> next_unconsumed(Terminal&);

    bool              m_want_tick = false;
    EventParser       m_parser;
    std::queue<Event> m_events;
};
}  // namespace trux::input
