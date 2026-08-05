#pragma once

#include <algorithm>
#include <iterator>
#include <vector>

namespace trux::focus {

using FocusID = const void*;

class FocusManager {
public:
    void begin_frame() { m_order.clear(); }

    void register_focusable(FocusID id) {
        m_order.push_back(id);
        if(!m_focused) m_focused = id;
    }

    void end_frame() {
        if(m_order.empty()) {
            m_focused = nullptr;
            return;
        }
        if(std::find(m_order.begin(), m_order.end(), m_focused) ==
           m_order.end())
            m_focused = m_order.front();
    }

    [[nodiscard]]
    bool is_focused(FocusID id) const noexcept {
        return m_focused == id;
    }
    [[nodiscard]]
    FocusID focused() const noexcept {
        return m_focused;
    }

    void focus(FocusID id) { m_focused = id; }

    void focus_next() {
        if(m_order.empty()) return;
        auto it   = std::find(m_order.begin(), m_order.end(), m_focused);
        m_focused = (it == m_order.end() || std::next(it) == m_order.end())
                        ? m_order.front()
                        : *std::next(it);
    }

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
