/// @file focus_manager.hpp
/// @brief FocusManager: tracks which of the components pushed this
///        frame currently has keyboard focus, and lets Tab/Shift+Tab
///        advance it in registration order (used internally by
///        Renderer).

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>

namespace trux::focus {

/// Opaque identity of a focusable component. Renderer derives this
/// from the pushed component's address.
using FocusID = const void*;

/// Tracks Tab-traversal order and which focusable component currently
/// has focus. Renderer calls register_focusable() for each focusable
/// component pushed in a frame, then end_frame() once the frame's
/// registrations are complete; is_focused() / focus_next() /
/// focus_prev() are typically used during input dispatch.
class FocusManager {
public:
    /// Clears the registration order ahead of a new frame's
    /// register_focusable() calls. Does not change which id is
    /// currently focused.
    void begin_frame() { m_order.clear(); }

    /// Records `id` as focusable this frame, appending it to the
    /// traversal order. The first component registered in a frame
    /// becomes focused by default if nothing else is.
    void register_focusable(FocusID id) {
        m_order.push_back(id);
        if(!m_focused) m_focused = id;
    }

    /// Call once all of this frame's register_focusable() calls are
    /// done. If the previously-focused id is no longer registered
    /// (e.g. its component was removed), focus falls back to the
    /// first registered id, or to none if nothing is focusable.
    void end_frame() {
        if(m_order.empty()) {
            m_focused = nullptr;
            return;
        }
        if(std::find(m_order.begin(), m_order.end(), m_focused) ==
           m_order.end())
            m_focused = m_order.front();
    }

    /// Whether `id` currently has focus.
    [[nodiscard]]
    bool is_focused(FocusID id) const noexcept {
        return m_focused == id;
    }
    /// The currently focused id, or nullptr if none.
    [[nodiscard]]
    FocusID focused() const noexcept {
        return m_focused;
    }

    /// Explicitly sets the focused id, regardless of registration.
    void focus(FocusID id) { m_focused = id; }

    /// Advances focus to the next registered id after the currently
    /// focused one, wrapping to the first. A no-op if nothing is registered.
    void focus_next() {
        if(m_order.empty()) return;
        auto it   = std::find(m_order.begin(), m_order.end(), m_focused);
        m_focused = (it == m_order.end() || std::next(it) == m_order.end())
                        ? m_order.front()
                        : *std::next(it);
    }

    /// Moves focus to the registered id before the currently focused
    /// one, wrapping to the last. A no-op if nothing is registered.
    void focus_prev() {
        if(m_order.empty()) return;
        auto it   = std::find(m_order.begin(), m_order.end(), m_focused);
        m_focused = (it == m_order.end() || it == m_order.begin())
                        ? m_order.back()
                        : *std::prev(it);
    }

private:
    std::vector<FocusID> m_order;
    FocusID              m_focused{nullptr};
};
}  // namespace trux::focus
