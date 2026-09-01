#pragma once
#include "trux/input/event.hpp"

#include <chrono>
#include <optional>
#include <unordered_map>

namespace trux::input {

enum class ChordResult { NoMatch, Pending, Matched };

template <typename T> class Keymap {
public:
    void set_timeout(std::chrono::milliseconds ms) noexcept { m_timeout = ms; }

    [[nodiscard]] bool has_pending() const noexcept {
        return m_current != nullptr;
    }

    void bind(std::initializer_list<char32_t> chord, T value) {
        Node* node = &m_root;
        for(char32_t key : chord) node = &node->children[key];
        node->value = std::move(value);
    }

    [[nodiscard]]
    ChordResult feed(const Event& event, T& out) {
        if(event.kind != EventKind::Key || event.key_state == KeyState::Release)
            return ChordResult::NoMatch;

        char32_t key  = static_cast<char32_t>(event);
        Node*    node = m_current ? m_current : &m_root;

        auto it = node->children.find(key);
        if(it == node->children.end()) {
            bool was_pending = (node != &m_root);
            reset();
            if(was_pending) return feed(event, out);
            return ChordResult::NoMatch;
        }

        node       = &it->second;
        m_deadline = std::chrono::steady_clock::now() + m_timeout;

        if(node->children.empty()) {
            out = *node->value;
            reset();
            return ChordResult::Matched;
        }

        m_current = node;
        return ChordResult::Pending;
    }

    [[nodiscard]]
    ChordResult check_timeout(T& out) {
        if(!m_current) return ChordResult::NoMatch;
        if(std::chrono::steady_clock::now() < m_deadline)
            return ChordResult::NoMatch;

        Node* node = m_current;
        reset();
        if(node->value) {
            out = *node->value;
            return ChordResult::Matched;
        }
        return ChordResult::NoMatch;
    }

private:
    struct Node {
        std::unordered_map<char32_t, Node> children;
        std::optional<T>                   value;
    };

    void reset() noexcept { m_current = nullptr; }

    Node                                  m_root;
    Node*                                 m_current{nullptr};
    std::chrono::milliseconds             m_timeout{600};
    std::chrono::steady_clock::time_point m_deadline;
};

}  // namespace trux::input
