/// @file piece_table.hpp
/// @brief PieceTable: trux's own text buffer for multi-line editing. A
///        plain value type that owns its text, tracks byte positions,
///        answers line queries, and supports grouped undo/redo.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace trux::text {

/// A text buffer stored as a sequence of "pieces", each a slice of one
/// of two append-only byte buffers: the original text and everything
/// inserted since. Edits only split, trim, or add pieces; no text is
/// ever moved or overwritten, which makes undo a matter of restoring
/// an earlier piece list.
///
/// All positions are byte offsets in `[0, size()]`. Out-of-range
/// arguments are clamped, never rejected. Bytes are opaque except in
/// prev_char()/next_char(), which step over UTF-8 sequences, and in the
/// line queries, which split on '\n'.
///
/// Copying a PieceTable copies its text and its full history, so
/// components holding one can be freely moved or copied.
///
/// Undo model: consecutive edits belong to one group (one undo step)
/// until seal() is called. Callers seal on cursor moves, mode changes,
/// Enter, and so on. Edits made after undo() discard the redo history.
class PieceTable {
public:
    PieceTable() = default;

    /// Starts with `initial` as the text. It is not undoable.
    explicit PieceTable(std::string initial)
        : m_original(std::move(initial)) {
        if(!m_original.empty())
            m_pieces.push_back({Source::Original, 0, m_original.size()});
    }

    /// Total length in bytes.
    [[nodiscard]]
    size_t size() const noexcept {
        size_t n = 0;
        for(const auto& p : m_pieces) n += p.len;
        return n;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return m_pieces.empty();
    }

    /// The byte at `pos`, or '\0' if `pos >= size()`.
    [[nodiscard]]
    char at(size_t pos) const noexcept {
        size_t acc = 0;
        for(const auto& p : m_pieces) {
            if(pos < acc + p.len) return view(p)[pos - acc];
            acc += p.len;
        }
        return '\0';
    }

    /// Copies `[pos, pos + len)` out as a string.
    [[nodiscard]]
    std::string substr(size_t pos, size_t len = npos) const {
        std::string out;
        size_t      total = size();
        if(pos >= total) return out;
        len = std::min(len, total - pos);
        out.reserve(len);
        size_t acc = 0;
        for(const auto& p : m_pieces) {
            size_t a = acc, b = acc + p.len;
            acc = b;
            if(b <= pos) continue;
            if(a >= pos + len) break;
            size_t from = std::max(a, pos) - a;
            size_t to   = std::min(b, pos + len) - a;
            out.append(view(p).substr(from, to - from));
        }
        return out;
    }

    /// The whole text.
    [[nodiscard]]
    std::string str() const {
        return substr(0);
    }

    /// Inserts `text` at `pos`.
    void insert(size_t pos, std::string_view text) {
        if(text.empty()) return;
        pos = std::min(pos, size());
        begin_edit();

        size_t add_start = m_add.size();
        m_add.append(text);

        size_t acc = 0, i = 0;
        for(; i < m_pieces.size(); i++) {
            if(pos <= acc + m_pieces[i].len) break;
            acc += m_pieces[i].len;
        }

        Piece fresh{Source::Add, add_start, text.size()};
        if(i == m_pieces.size()) {
            m_pieces.push_back(fresh);
            return;
        }

        Piece& p   = m_pieces[i];
        size_t off = pos - acc;
        if(off == p.len) {
            // Typing at the end of the most recent insertion: grow it.
            if(p.src == Source::Add && p.start + p.len == add_start) {
                p.len += text.size();
            } else {
                m_pieces.insert(m_pieces.begin() + static_cast<long>(i) + 1,
                                fresh);
            }
        } else if(off == 0) {
            m_pieces.insert(m_pieces.begin() + static_cast<long>(i), fresh);
        } else {
            Piece right{p.src, p.start + off, p.len - off};
            p.len = off;
            m_pieces.insert(m_pieces.begin() + static_cast<long>(i) + 1,
                            {fresh, right});
        }
    }

    /// Removes `[pos, pos + len)`.
    void erase(size_t pos, size_t len) {
        size_t total = size();
        if(pos >= total || len == 0) return;
        len = std::min(len, total - pos);
        begin_edit();

        std::vector<Piece> out;
        out.reserve(m_pieces.size() + 1);
        size_t acc = 0, end = pos + len;
        for(const auto& p : m_pieces) {
            size_t a = acc, b = acc + p.len;
            acc = b;
            if(b <= pos || a >= end) {
                out.push_back(p);
                continue;
            }
            if(a < pos) out.push_back({p.src, p.start, pos - a});
            if(b > end) out.push_back({p.src, p.start + (end - a), b - end});
        }
        m_pieces = std::move(out);
    }

    /// Replaces `[pos, pos + len)` with `text` as a single edit.
    void replace(size_t pos, size_t len, std::string_view text) {
        erase(pos, len);
        insert(pos, text);
    }

    // ----- undo / redo -----------------------------------------------

    /// Ends the current undo group; the next edit starts a new one.
    void seal() noexcept { m_open = false; }

    [[nodiscard]]
    bool can_undo() const noexcept {
        return m_open || !m_undo.empty();
    }
    [[nodiscard]]
    bool can_redo() const noexcept {
        return !m_redo.empty();
    }

    /// Reverts the most recent group. Returns false if there is none.
    bool undo() {
        m_open = false;
        if(m_undo.empty()) return false;
        m_redo.push_back(std::move(m_pieces));
        m_pieces = std::move(m_undo.back());
        m_undo.pop_back();
        m_lines_dirty = true;
        return true;
    }

    /// Re-applies the most recently undone group.
    bool redo() {
        m_open = false;
        if(m_redo.empty()) return false;
        m_undo.push_back(std::move(m_pieces));
        m_pieces = std::move(m_redo.back());
        m_redo.pop_back();
        m_lines_dirty = true;
        return true;
    }

    /// Caps how many undo groups are kept (oldest dropped first).
    void set_history_limit(size_t groups) {
        m_limit = std::max<size_t>(groups, 1);
        while(m_undo.size() > m_limit) m_undo.pop_front();
    }

    // ----- characters ------------------------------------------------

    /// Start of the UTF-8 character before `pos` (0 stays 0).
    [[nodiscard]]
    size_t prev_char(size_t pos) const noexcept {
        pos = std::min(pos, size());
        if(pos == 0) return 0;
        size_t i = pos - 1;
        while(i > 0 && (static_cast<unsigned char>(at(i)) & 0xC0) == 0x80) i--;
        return i;
    }

    /// Start of the UTF-8 character after the one at `pos`.
    [[nodiscard]]
    size_t next_char(size_t pos) const noexcept {
        size_t n = size();
        if(pos >= n) return n;
        size_t i = pos + 1;
        while(i < n && (static_cast<unsigned char>(at(i)) & 0xC0) == 0x80) i++;
        return i;
    }

    // ----- lines -----------------------------------------------------
    // A text with k '\n' bytes has k + 1 lines; the empty text has one
    // empty line. Line indexes are rebuilt lazily after edits.

    [[nodiscard]]
    size_t line_count() const {
        index();
        return m_lines.size();
    }

    /// Byte offset where line `n` starts (clamped to the last line).
    [[nodiscard]]
    size_t line_start(size_t n) const {
        index();
        return m_lines[std::min(n, m_lines.size() - 1)];
    }

    /// Byte offset of line `n`'s end, before its '\n' (or at size()).
    [[nodiscard]]
    size_t line_end(size_t n) const {
        index();
        n = std::min(n, m_lines.size() - 1);
        return n + 1 < m_lines.size() ? m_lines[n + 1] - 1 : size();
    }

    /// Index of the line containing `pos`.
    [[nodiscard]]
    size_t line_of(size_t pos) const {
        index();
        auto it = std::upper_bound(m_lines.begin(), m_lines.end(), pos);
        return static_cast<size_t>(it - m_lines.begin()) - 1;
    }

    /// Line `n` without its '\n'.
    [[nodiscard]]
    std::string line_text(size_t n) const {
        size_t s = line_start(n);
        return substr(s, line_end(n) - s);
    }

    static constexpr size_t npos = static_cast<size_t>(-1);

private:
    enum class Source : uint8_t { Original, Add };
    struct Piece {
        Source src;
        size_t start;
        size_t len;
    };

    [[nodiscard]]
    std::string_view view(const Piece& p) const noexcept {
        const std::string& buf = p.src == Source::Original ? m_original : m_add;
        return std::string_view(buf).substr(p.start, p.len);
    }

    /// Snapshots the piece list once per undo group, before its first edit.
    void begin_edit() {
        m_lines_dirty = true;
        m_redo.clear();
        if(m_open) return;
        m_undo.push_back(m_pieces);
        if(m_undo.size() > m_limit) m_undo.pop_front();
        m_open = true;
    }

    void index() const {
        if(!m_lines_dirty) return;
        m_lines.assign(1, 0);
        size_t acc = 0;
        for(const auto& p : m_pieces) {
            auto v = view(p);
            for(size_t i = 0; i < v.size(); i++)
                if(v[i] == '\n') m_lines.push_back(acc + i + 1);
            acc += v.size();
        }
        m_lines_dirty = false;
    }

    std::string                     m_original;
    std::string                     m_add;
    std::vector<Piece>              m_pieces;
    std::deque<std::vector<Piece>>  m_undo;
    std::vector<std::vector<Piece>> m_redo;
    size_t                          m_limit{1000};
    bool                            m_open{false};

    mutable std::vector<size_t>     m_lines{0};
    mutable bool                    m_lines_dirty{true};
};

}  // namespace trux::text
