/// @file keymap.hpp
/// @brief Keymap: a trie of key chords (multi-key sequences like
///        `g g` or `Ctrl+K Ctrl+S`) mapping to application-defined
///        values, with timeout-based resolution of ambiguous prefixes.

#pragma once
#include "trux/input/event.hpp"

#include <chrono>
#include <optional>
#include <unordered_map>

namespace trux::input {

/// Outcome of feeding one event to a Keymap.
enum class ChordResult { NoMatch, Pending, Matched };

/// Maps sequences of key events ("chords") to values of type `T`.
/// Chords are matched incrementally via feed(): a prefix that could
/// still extend to a longer bound chord returns Pending, and the
/// caller should call check_timeout() after m_timeout has elapsed to
/// resolve it (as a shorter match, if any, or NoMatch).
template <typename T> class Keymap {
public:
    /// Sets how long a Pending chord waits for its next key before
    /// check_timeout() will resolve it.
    void set_timeout(std::chrono::milliseconds ms) noexcept { m_timeout = ms; }

    /// Whether a chord prefix is currently pending completion.
    [[nodiscard]] bool has_pending() const noexcept {
        return m_current != nullptr;
    }

    /// Binds the key sequence `chord` to `value`, creating any
    /// intermediate trie nodes needed.
    void bind(std::initializer_list<char32_t> chord, T value) {
        Node* node = &m_root;
        for(char32_t key : chord) node = &node->children[key];
        node->value = std::move(value);
    }

    /// Advances the chord trie with one key `event`. Non-key events
    /// and releases are ignored (NoMatch). On a full match, writes
    /// the bound value to `out` and returns Matched. On a partial
    /// match that could still extend, returns Pending. If `event`
    /// doesn't continue the current pending chord, the pending state
    /// is dropped and the event is retried from the root — so a key
    /// that doesn't extend one chord can still start another.
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

    /// Call periodically (e.g. once per event-loop tick). If a chord
    /// prefix has been Pending longer than the configured timeout,
    /// resolves it: writes its bound value to `out` and returns
    /// Matched if the pending node itself has one, otherwise NoMatch.
    /// A no-op returning NoMatch if nothing is pending or the timeout
    /// hasn't elapsed.
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
    /// One node in the chord trie: children keyed by the next key in
    /// the sequence, and an optional bound value if a chord ends here.
    struct Node {
        std::unordered_map<char32_t, Node> children;
        std::optional<T>                   value;
    };

    /// Clears the in-progress chord, returning to the root.
    void reset() noexcept { m_current = nullptr; }

    Node                                  m_root;
    Node*                                 m_current{nullptr};
    std::chrono::milliseconds             m_timeout{600};
    std::chrono::steady_clock::time_point m_deadline;
};

}  // namespace trux::input
