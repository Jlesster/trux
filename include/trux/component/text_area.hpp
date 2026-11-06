/// @file text_area.hpp
/// @brief TextArea: a multi-line, vim-modal text editor component
///        backed by trux::text::PieceTable.

#pragma once

#include <algorithm>
#include <climits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <trux/component/border.hpp>
#include <trux/component/component_flags.hpp>
#include <trux/input/event.hpp>
#include <trux/input/key.hpp>
#include <trux/renderer/draw_command.hpp>
#include <trux/renderer/draw_command_buffer.hpp>
#include <trux/style/style.hpp>
#include <trux/text/piece_table.hpp>
#include <trux/util/util.hpp>

namespace trux::component {

/// A multi-line text editor with vim-style modal editing built into its
/// input handling. Like TextInput, it owns its state as plain public
/// fields, so `area.cursor = 0` or `area.mode = Mode::Insert` work
/// directly, and it plugs into push() and the `|` idiom unchanged.
///
/// Modes
///   Normal  motions h j k l w b e 0 ^ $ gg G (with counts), operators
///           d c y > < (doubled for whole lines, or followed by a
///           motion), x X D C s S Y J p P r u Ctrl-R, and the insert
///           entries i a I A o O. Ctrl-D/Ctrl-U/PageUp/PageDown scroll.
///   Insert  typing, Enter (auto-indents), Backspace/Delete (Ctrl for
///           words), arrows, Home/End, Tab (indent), Shift-Tab (dedent),
///           and Paste. Esc returns to Normal.
///
/// Tab: in Insert mode Tab is consumed to indent. In Normal mode it is
/// not, so focus traversal still works. Esc in Normal mode with nothing
/// pending is also left unconsumed, so an app can use it to leave the
/// edit view. Tab reaches handle() via captures_tab(), which Renderer
/// checks before it does focus traversal.
///
/// Undo: each Normal-mode command is one undo step; an Insert session
/// (up to Esc, a cursor move or Enter) is one step.
///
/// Not in v1: visual mode, `.` repeat, named registers, soft wrap,
/// search, mouse.
struct TextArea {
    enum class Mode : uint8_t { Normal, Insert };

    TextArea() = default;

    /// Starts with `initial` as the text and the cursor at its start.
    explicit TextArea(std::string initial) : buf(std::move(initial)) {}

    /// Replaces the text; clears undo history, view and pending input.
    void set_text(std::string t) {
        buf    = text::PieceTable(std::move(t));
        cursor = 0;
        m_top = m_left = 0;
        m_want_col     = -1;
        clear_pending();
    }

    /// The whole text.
    [[nodiscard]]
    std::string text() const {
        return buf.str();
    }

    /// Zero-based line the cursor is on.
    [[nodiscard]]
    size_t row() const {
        clamp();
        return buf.line_of(cursor);
    }

    /// Display column of the cursor within its line.
    [[nodiscard]]
    int col() const {
        clamp();
        return cursor_col();
    }

    /// "NORMAL" or "INSERT", for status lines.
    [[nodiscard]]
    const char* mode_name() const noexcept {
        return mode == Mode::Insert ? "INSERT" : "NORMAL";
    }

    /// The text being edited.
    text::PieceTable buf;
    /// Byte offset of the cursor; clamped during build()/handle().
    mutable size_t cursor = 0;
    Mode           mode   = Mode::Normal;

    /// Navigation and yank only; edits and insert entries are ignored.
    bool read_only = false;
    /// Whether build() draws the cursor cell (set from focus each frame).
    bool show_cursor = true;
    /// Enter, o and O copy the current line's indentation.
    bool auto_indent = true;
    /// Tab inserts spaces (true) or a '\t' (false).
    bool expand_tab = true;
    /// Columns per tab stop and per indent level.
    int tab_width = 4;

    ComponentFlags flags{};
    style::Style   text_style{style::Style::None};
    style::Style   cursor_style{style::Style::Reverse};
    style::Color   border_color{255, 255, 255, 255};
    /// Border color used when unfocused (see component::BorderColor).
    std::optional<style::Color> border_color_fixed;
    /// Border color used while focused; falls back to
    /// `border_color_fixed` when unset.
    std::optional<style::Color> border_color_active;

    /// First visible line and column; recomputed by build().
    mutable int m_top{0};
    mutable int m_left{0};
    /// Height of the last build(), used for page scrolling.
    mutable int m_view_h{10};

    /// Clamps the cursor, scrolls to keep it visible, and draws the
    /// visible slice of the text plus the cursor cell.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();
        if(w <= 0 || h <= 0) return;
        m_view_h = h;

        clamp();
        const size_t lines  = buf.line_count();
        const size_t cl     = buf.line_of(cursor);
        const std::string line_str = buf.line_text(cl);
        const int ccol = col_of(line_str, cursor - buf.line_start(cl));
        int cgw = 1;
        if(cursor < buf.line_end(cl)) {
            cgw = glyph(line_str, cursor - buf.line_start(cl), ccol).width;
            cgw = std::min(cgw, 2);
        }

        if(static_cast<int>(cl) < m_top) m_top = static_cast<int>(cl);
        if(static_cast<int>(cl) >= m_top + h)
            m_top = static_cast<int>(cl) - h + 1;
        m_top = std::clamp(m_top, 0, static_cast<int>(lines) - 1);

        if(ccol < m_left) m_left = ccol;
        if(ccol + cgw > m_left + w) m_left = ccol + cgw - w;
        m_left = std::max(m_left, 0);

        // Rows are sized once up front: string_views into them must not
        // be invalidated by a later reallocation.
        m_rows.assign(static_cast<size_t>(h), std::string{});
        for(int r = 0; r < h; r++) {
            size_t line = static_cast<size_t>(m_top + r);
            if(line >= lines) break;
            m_rows[static_cast<size_t>(r)] =
                slice(expand(buf.line_text(line)), m_left, w);
        }

        auto pos = content.position();
        for(int r = 0; r < h; r++) {
            const auto& row = m_rows[static_cast<size_t>(r)];
            if(row.empty()) continue;
            cmd.push(renderer::DrawText{{pos.x, pos.y + r}, row, text_style});
        }

        if(show_cursor) {
            m_cursor_cell = " ";
            if(cursor < buf.line_end(cl)) {
                size_t off = cursor - buf.line_start(cl);
                auto   g   = glyph(line_str, off, ccol);
                if(g.kind == Glyph::Normal)
                    m_cursor_cell = line_str.substr(off, static_cast<size_t>(g.len));
            }
            cmd.push(renderer::DrawText{
                {pos.x + ccol - m_left, pos.y + static_cast<int>(cl) - m_top},
                m_cursor_cell, cursor_style});
        }
    }

    /// Asks the renderer to offer Tab/Shift+Tab to handle() instead of
    /// using them for focus traversal. True only while Tab indents.
    [[nodiscard]]
    bool captures_tab() const noexcept {
        return mode == Mode::Insert && !read_only;
    }

    /// Handles a key or paste event according to the current mode.
    /// Returns whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(read_only && mode == Mode::Insert) mode = Mode::Normal;

        if(event.kind == input::EventKind::Paste) {
            if(mode != Mode::Insert) return false;
            insert_text(normalise_paste(event.paste));
            buf.seal();
            return true;
        }
        if(event.kind != input::EventKind::Key) return false;
        if(event.key_state == input::KeyState::Release) return false;

        clamp();
        return mode == Mode::Insert ? handle_insert(event) : handle_normal(event);
    }

private:
    // ----- display geometry -------------------------------------------

    struct Glyph {
        enum Kind : uint8_t { Normal, Tab, Invalid } kind{Normal};
        int len{1};
        int width{1};
    };

    int tw() const noexcept { return std::max(1, tab_width); }

    /// Describes the character at byte `i` of `s`, which sits at display
    /// column `col` (tabs depend on it).
    Glyph glyph(std::string_view s, size_t i, int col) const {
        auto c = static_cast<unsigned char>(s[i]);
        if(c == '\t') return {Glyph::Tab, 1, tw() - col % tw()};
        if(c < 0x20 || c == 0x7f) return {Glyph::Invalid, 1, 1};
        auto d = util::decode_utf8(s.substr(i));
        if(!d) return {Glyph::Invalid, 1, 1};
        return {Glyph::Normal, d->second, std::max(1, util::glyph_width(d->first))};
    }

    int col_of(std::string_view line, size_t off) const {
        int    col = 0;
        size_t i   = 0;
        while(i < off && i < line.size()) {
            auto g = glyph(line, i, col);
            col += g.width;
            i += static_cast<size_t>(g.len);
        }
        return col;
    }

    /// Byte offset of the character covering display column `col`.
    size_t byte_at_col(std::string_view line, int col) const {
        int    c = 0;
        size_t i = 0;
        while(i < line.size()) {
            auto g = glyph(line, i, c);
            if(c + g.width > col) break;
            c += g.width;
            i += static_cast<size_t>(g.len);
        }
        return i;
    }

    /// The line as drawn: tabs become spaces, stray control bytes '?'.
    std::string expand(std::string_view line) const {
        std::string out;
        int         col = 0;
        size_t      i   = 0;
        while(i < line.size()) {
            auto g = glyph(line, i, col);
            if(g.kind == Glyph::Tab) out.append(static_cast<size_t>(g.width), ' ');
            else if(g.kind == Glyph::Invalid) out.push_back('?');
            else out.append(line.substr(i, static_cast<size_t>(g.len)));
            col += g.width;
            i += static_cast<size_t>(g.len);
        }
        return out;
    }

    /// Columns [left, left + width) of an already-expanded line.
    static std::string slice(std::string_view s, int left, int width) {
        std::string out;
        int         col = 0;
        size_t      i   = 0;
        while(i < s.size()) {
            auto d   = util::decode_utf8(s.substr(i));
            int  len = d ? d->second : 1;
            int  gw  = d ? std::max(1, util::glyph_width(d->first)) : 1;
            if(col >= left) {
                if(col + gw > left + width) break;
                out.append(s.substr(i, static_cast<size_t>(len)));
            } else if(col + gw > left) {
                out.append(static_cast<size_t>(col + gw - left), ' ');
            }
            col += gw;
            i += static_cast<size_t>(len);
        }
        return out;
    }

    int cursor_col() const {
        size_t l = buf.line_of(cursor);
        return col_of(buf.line_text(l), cursor - buf.line_start(l));
    }

    // ----- cursor helpers ---------------------------------------------

    static bool is_cont(char c) noexcept {
        return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
    }

    /// Keeps the cursor on a character start inside the text; in Normal
    /// mode it also may not rest past a line's last character.
    void clamp() const {
        cursor = std::min(cursor, buf.size());
        while(cursor > 0 && cursor < buf.size() && is_cont(buf.at(cursor)))
            cursor--;
        if(mode == Mode::Normal) {
            size_t l = buf.line_of(cursor);
            size_t s = buf.line_start(l), e = buf.line_end(l);
            if(cursor >= e && e > s) cursor = buf.prev_char(e);
        }
    }

    void set_cursor(size_t pos) {
        cursor     = pos;
        m_want_col = -1;
        clamp();
    }

    size_t cur_line() const { return buf.line_of(cursor); }

    size_t first_nonblank(size_t line) const {
        size_t p = buf.line_start(line), e = buf.line_end(line);
        while(p < e && (buf.at(p) == ' ' || buf.at(p) == '\t')) p++;
        return p;
    }

    std::string indent_of(size_t line, size_t upto = std::string::npos) const {
        size_t s = buf.line_start(line), e = std::min(buf.line_end(line), upto);
        size_t p = s;
        while(p < e && (buf.at(p) == ' ' || buf.at(p) == '\t')) p++;
        return buf.substr(s, p - s);
    }

    /// Byte offset on `line` at the remembered display column.
    size_t at_want_col(size_t line) {
        if(m_want_col < 0) m_want_col = cursor_col();
        size_t off = byte_at_col(buf.line_text(line), m_want_col);
        return buf.line_start(line) + off;
    }

    // ----- character classes and word motions -------------------------

    int cls(size_t p) const {
        auto c = static_cast<unsigned char>(buf.at(p));
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') return 0;
        if(c >= 0x80 || c == '_' || (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            return 1;
        return 2;
    }

    size_t word_fwd(size_t p) const {
        size_t n = buf.size();
        if(p >= n) return n;
        int c = cls(p);
        if(c != 0)
            while(p < n && cls(p) == c) p++;
        bool moved = false;
        while(p < n && cls(p) == 0) {
            if(moved && buf.at(p) == '\n' && buf.at(p - 1) == '\n') break;
            p++;
            moved = true;
        }
        return p;
    }

    size_t word_back(size_t p) const {
        if(p == 0) return 0;
        p--;
        while(p > 0 && cls(p) == 0) {
            if(buf.at(p) == '\n' && buf.at(p - 1) == '\n') return p;
            p--;
        }
        int c = cls(p);
        while(p > 0 && cls(p - 1) == c) p--;
        while(p > 0 && is_cont(buf.at(p))) p--;
        return p;
    }

    size_t word_end(size_t p) const {
        size_t n = buf.size();
        p        = buf.next_char(p);
        while(p < n && cls(p) == 0) p++;
        if(p >= n) return n == 0 ? 0 : buf.prev_char(n);
        int c = cls(p);
        while(p + 1 < n && cls(p + 1) == c) p++;
        while(p > 0 && is_cont(buf.at(p))) p--;
        return p;
    }

    // ----- motions -----------------------------------------------------

    struct Motion {
        size_t pos{};
        bool   linewise{false};
        bool   inclusive{false};
        bool   vertical{false};
        bool   to_eol{false};
    };

    /// Computes where motion key `k` goes. `count` is 0 when the user
    /// gave none. `op` is true when an operator is pending.
    std::optional<Motion> motion(char32_t k, int count, bool op) {
        const int    n    = std::max(1, count);
        const size_t line = cur_line();
        const size_t last = buf.line_count() - 1;
        const size_t ls = buf.line_start(line), le = buf.line_end(line);

        switch(k) {
            case 'h': {
                size_t p = cursor;
                for(int i = 0; i < n && p > ls; i++) p = buf.prev_char(p);
                return Motion{p};
            }
            case 'l':
            case ' ': {
                size_t limit = op ? le : (le > ls ? buf.prev_char(le) : ls);
                size_t p     = cursor;
                for(int i = 0; i < n && p < limit; i++) p = buf.next_char(p);
                return Motion{p};
            }
            case 'j':
            case 'k': {
                size_t target = k == 'j'
                                    ? std::min(line + static_cast<size_t>(n), last)
                                    : (line >= static_cast<size_t>(n)
                                           ? line - static_cast<size_t>(n)
                                           : 0);
                if(target == line && op) return std::nullopt;
                Motion m{at_want_col(target)};
                m.linewise = m.vertical = true;
                return m;
            }
            case '0': return Motion{ls};
            case '^': return Motion{first_nonblank(line)};
            case '$': {
                size_t tl = std::min(line + static_cast<size_t>(n - 1), last);
                size_t s = buf.line_start(tl), e = buf.line_end(tl);
                Motion m{e > s ? buf.prev_char(e) : s};
                m.inclusive = m.to_eol = true;
                return m;
            }
            case 'g':
            case 'G': {
                size_t target = k == 'g'
                                    ? (count ? std::min<size_t>(count - 1, last) : 0)
                                    : (count ? std::min<size_t>(count - 1, last) : last);
                Motion m{first_nonblank(target)};
                m.linewise = true;
                return m;
            }
            case 'w': {
                size_t p = cursor;
                for(int i = 0; i < n; i++) p = word_fwd(p);
                // dw/yw on a line's last word stops at the line end.
                if(op && buf.line_of(std::min(p, buf.size())) > line)
                    p = le;
                return Motion{p};
            }
            case 'b': {
                size_t p = cursor;
                for(int i = 0; i < n; i++) p = word_back(p);
                return Motion{p};
            }
            case 'e': {
                size_t p = cursor;
                for(int i = 0; i < n; i++) p = word_end(p);
                Motion m{p};
                m.inclusive = true;
                return m;
            }
            default: return std::nullopt;
        }
    }

    void commit_motion(const Motion& m) {
        cursor = m.pos;
        if(m.vertical) {
            clamp();
        } else {
            m_want_col = m.to_eol ? INT_MAX : -1;
            clamp();
        }
    }

    // ----- edits -------------------------------------------------------

    std::string indent_unit() const {
        return expand_tab ? std::string(static_cast<size_t>(tw()), ' ')
                          : std::string("\t");
    }

    void insert_text(const std::string& s) {
        if(s.empty()) return;
        buf.insert(cursor, s);
        cursor += s.size();
        m_want_col = -1;
    }

    static std::string normalise_paste(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for(size_t i = 0; i < in.size(); i++) {
            auto c = static_cast<unsigned char>(in[i]);
            if(c == '\r') {
                if(i + 1 < in.size() && in[i + 1] == '\n') continue;
                out.push_back('\n');
            } else if(c == '\n' || c == '\t' || c >= 0x20) {
                out.push_back(static_cast<char>(c));
            }
        }
        return out;
    }

    /// Prepends one indent level to `line` (empty lines are skipped).
    size_t indent_line(size_t line) {
        if(buf.line_end(line) == buf.line_start(line)) return 0;
        auto unit = indent_unit();
        buf.insert(buf.line_start(line), unit);
        return unit.size();
    }

    /// Removes up to one indent level from `line`; returns bytes removed.
    size_t dedent_line(size_t line) {
        size_t s = buf.line_start(line), e = buf.line_end(line);
        size_t n = 0;
        if(s < e && buf.at(s) == '\t') {
            n = 1;
        } else {
            while(s + n < e && n < static_cast<size_t>(tw()) && buf.at(s + n) == ' ')
                n++;
        }
        buf.erase(s, n);
        return n;
    }

    void yank(std::string text, bool linewise) {
        m_reg           = std::move(text);
        m_reg_linewise = linewise;
    }

    void charwise_op(char32_t op, size_t a, size_t b) {
        if(op == '>' || op == '<') {
            linewise_op(op, buf.line_of(a), buf.line_of(b > a ? b - 1 : b));
            return;
        }
        if(op == 'y') {
            yank(buf.substr(a, b - a), false);
            set_cursor(a);
            return;
        }
        if(read_only) return;
        if(a == b) {
            if(op == 'c') enter_insert(a);
            return;
        }
        buf.seal();
        yank(buf.substr(a, b - a), false);
        buf.erase(a, b - a);
        if(op == 'c') {
            mode = Mode::Insert;
            set_cursor(a);
        } else {
            buf.seal();
            set_cursor(a);
        }
    }

    void linewise_op(char32_t op, size_t l1, size_t l2) {
        const size_t ls = buf.line_start(l1), le = buf.line_end(l2);
        if(op == 'y') {
            yank(buf.substr(ls, le - ls), true);
            if(l1 < cur_line()) set_cursor(ls);
            return;
        }
        if(read_only) return;
        buf.seal();
        if(op == '>' || op == '<') {
            for(size_t l = l2 + 1; l-- > l1;)
                op == '>' ? indent_line(l) : dedent_line(l);
            buf.seal();
            set_cursor(first_nonblank(l1));
            return;
        }
        yank(buf.substr(ls, le - ls), true);
        if(op == 'c') {
            std::string indent = auto_indent ? indent_of(l1) : std::string{};
            buf.erase(ls, le - ls);
            buf.insert(ls, indent);
            mode = Mode::Insert;
            set_cursor(ls + indent.size());
            return;
        }
        const size_t last = buf.line_count() - 1;
        size_t a, b;
        if(l2 == last && l1 > 0) {
            a = ls - 1;
            b = buf.size();
        } else {
            a = ls;
            b = std::min(le + 1, buf.size());
        }
        buf.erase(a, b - a);
        buf.seal();
        set_cursor(first_nonblank(std::min(l1, buf.line_count() - 1)));
    }

    void apply_operator(char32_t op, size_t from, const Motion& m) {
        size_t a = std::min(from, m.pos), b = std::max(from, m.pos);
        if(m.linewise) {
            linewise_op(op, buf.line_of(a), buf.line_of(b));
            return;
        }
        if(m.inclusive && b < buf.size() && buf.at(b) != '\n')
            b = buf.next_char(b);
        charwise_op(op, a, b);
    }

    void paste(bool after, int count) {
        if(read_only || m_reg.empty()) return;
        buf.seal();
        const size_t line = cur_line();
        if(m_reg_linewise) {
            std::string block;
            for(int i = 0; i < count; i++) block += (i ? "\n" : "") + m_reg;
            size_t at;
            if(after) {
                at = buf.line_end(line);
                buf.insert(at, "\n" + block);
                at += 1;
            } else {
                at = buf.line_start(line);
                buf.insert(at, block + "\n");
            }
            buf.seal();
            set_cursor(first_nonblank(buf.line_of(at)));
            return;
        }
        std::string block;
        for(int i = 0; i < count; i++) block += m_reg;
        size_t at = cursor;
        if(after && cursor < buf.line_end(line)) at = buf.next_char(cursor);
        buf.insert(at, block);
        buf.seal();
        set_cursor(buf.prev_char(at + block.size()));
    }

    void join_lines(int count) {
        if(read_only) return;
        buf.seal();
        for(int i = 0; i < std::max(1, count - 1); i++) {
            size_t l = cur_line();
            if(l + 1 >= buf.line_count()) break;
            size_t le = buf.line_end(l);
            size_t p  = le + 1;
            while(p < buf.size() && (buf.at(p) == ' ' || buf.at(p) == '\t')) p++;
            bool next_empty = p >= buf.size() || buf.at(p) == '\n';
            bool add_space  = !next_empty && le > buf.line_start(l);
            buf.erase(le, p - le);
            if(add_space) buf.insert(le, " ");
            cursor = le;
        }
        buf.seal();
        set_cursor(cursor);
    }

    // ----- modes ---------------------------------------------------------

    void enter_insert(size_t pos) {
        if(read_only) return;
        buf.seal();
        mode = Mode::Insert;
        set_cursor(pos);
    }

    void leave_insert() {
        buf.seal();
        mode = Mode::Normal;
        size_t s = buf.line_start(cur_line());
        set_cursor(cursor > s ? buf.prev_char(cursor) : cursor);
    }

    void open_line(bool below) {
        if(read_only) return;
        buf.seal();
        const size_t line   = cur_line();
        std::string  indent = auto_indent ? indent_of(line) : std::string{};
        mode                = Mode::Insert;
        if(below) {
            size_t at = buf.line_end(line);
            buf.insert(at, "\n" + indent);
            set_cursor(at + 1 + indent.size());
        } else {
            size_t at = buf.line_start(line);
            buf.insert(at, indent + "\n");
            set_cursor(at + indent.size());
        }
    }

    void clear_pending() {
        m_count = 0;
        m_op    = 0;
        m_pend  = 0;
    }

    void move_lines(int delta) {
        size_t line = cur_line();
        size_t last = buf.line_count() - 1;
        size_t target =
            delta >= 0 ? std::min(line + static_cast<size_t>(delta), last)
                       : (line >= static_cast<size_t>(-delta)
                              ? line - static_cast<size_t>(-delta)
                              : 0);
        cursor = at_want_col(target);
        clamp();
    }

    // ----- Normal mode -----------------------------------------------------

    /// Maps navigation keys onto their vim letters; 0 if unmapped.
    static char32_t normal_key(const input::Event& e) {
        switch(e.code) {
            case input::Key::Left: return 'h';
            case input::Key::Right: return 'l';
            case input::Key::Up: return 'k';
            case input::Key::Down: return 'j';
            case input::Key::Home: return '0';
            case input::Key::End: return '$';
            case input::Key::Backspace: return 'h';
            case input::Key::Delete: return 'x';
            default: break;
        }
        if(e.code >= 32 && e.code < input::KeyBackSpace) return e.code;
        return 0;
    }

    bool handle_normal(const input::Event& e) {
        const bool ctrl = e.mods.has(input::Mod::Ctrl);
        const bool alt  = e.mods.has(input::Mod::Alt);

        if(e.code == input::Key::Escape) {
            bool had = m_count || m_op || m_pend;
            clear_pending();
            return had;
        }
        if(alt) return false;

        if(e.code == input::Key::PageDown || e.code == input::Key::PageUp) {
            clear_pending();
            move_lines(e.code == input::Key::PageDown ? m_view_h : -m_view_h);
            return true;
        }
        if(ctrl) {
            const int cnt = std::max(1, m_count);
            clear_pending();
            switch(e.code) {
                case 'r':
                    for(int i = 0; i < cnt; i++) buf.redo();
                    set_cursor(cursor);
                    return true;
                case 'd': move_lines(std::max(1, m_view_h / 2)); return true;
                case 'u': move_lines(-std::max(1, m_view_h / 2)); return true;
                default: return false;
            }
        }

        char32_t k = normal_key(e);
        if(k == 0) return false;  // Tab, Enter, F-keys...

        // r{char}
        if(m_pend == 'r') {
            clear_pending();
            if(e.code >= 32 && e.code < input::KeyBackSpace && !read_only &&
               cursor < buf.line_end(cur_line())) {
                buf.seal();
                size_t end = buf.next_char(cursor);
                buf.replace(cursor, end - cursor, util::encode_utf8(e.code));
                buf.seal();
            }
            return true;
        }

        // gg
        if(m_pend == 'g') {
            m_pend = 0;
            if(k == 'g') return run_motion('g');
            clear_pending();
            return true;
        }

        if((k >= '1' && k <= '9') || (k == '0' && m_count > 0)) {
            m_count = std::min(m_count * 10 + static_cast<int>(k - '0'), 100000);
            return true;
        }

        if(k == 'g') {
            m_pend = 'g';
            return true;
        }

        // Operators: d c y > < , doubled for whole lines.
        if(k == 'd' || k == 'c' || k == 'y' || k == '>' || k == '<') {
            if(m_op == 0) {
                m_op = k;
                return true;
            }
            if(m_op == k) {
                size_t l = cur_line();
                size_t l2 = std::min(l + static_cast<size_t>(std::max(1, m_count)) - 1,
                                     buf.line_count() - 1);
                char32_t op = m_op;
                clear_pending();
                linewise_op(op, l, l2);
                return true;
            }
            clear_pending();
            return true;
        }

        if(run_motion(k)) return true;
        if(m_op) {  // operator followed by a non-motion
            clear_pending();
            return true;
        }
        return run_command(k);
    }

    /// Runs a motion, applying the pending operator if there is one.
    /// Returns false (and changes nothing) if `k` is not a motion.
    bool run_motion(char32_t k) {
        const bool op = m_op != 0;
        int        cnt = m_count;
        char32_t   mk  = k;
        // cw behaves like ce when on a word.
        if(m_op == 'c' && k == 'w' && cls(cursor) != 0) mk = 'e';
        auto m = motion(mk, cnt, op);
        if(!m) {
            if(!is_motion_key(k)) return false;
            clear_pending();
            return true;
        }
        const size_t from = cursor;
        const char32_t o  = m_op;
        clear_pending();
        if(o) apply_operator(o, from, *m);
        else commit_motion(*m);
        return true;
    }

    static bool is_motion_key(char32_t k) {
        switch(k) {
            case 'h': case 'l': case ' ': case 'j': case 'k': case '0':
            case '^': case '$': case 'g': case 'G': case 'w': case 'b':
            case 'e': return true;
            default: return false;
        }
    }

    bool run_command(char32_t k) {
        const int cnt = std::max(1, m_count);
        clear_pending();
        const size_t line = cur_line();
        const size_t ls = buf.line_start(line), le = buf.line_end(line);

        switch(k) {
            case 'i': enter_insert(cursor); break;
            case 'a': enter_insert(cursor < le ? buf.next_char(cursor) : cursor); break;
            case 'I': enter_insert(first_nonblank(line)); break;
            case 'A': enter_insert(le); break;
            case 'o': open_line(true); break;
            case 'O': open_line(false); break;
            case 'x': {
                size_t end = cursor;
                for(int i = 0; i < cnt && end < le; i++) end = buf.next_char(end);
                charwise_op('d', cursor, end);
                break;
            }
            case 'X': {
                size_t start = cursor;
                for(int i = 0; i < cnt && start > ls; i++) start = buf.prev_char(start);
                charwise_op('d', start, cursor);
                break;
            }
            case 'D': charwise_op('d', cursor, le); break;
            case 'C': charwise_op('c', cursor, le); break;
            case 's': {
                size_t end = cursor;
                for(int i = 0; i < cnt && end < le; i++) end = buf.next_char(end);
                charwise_op('c', cursor, end);
                break;
            }
            case 'S':
                linewise_op('c', line,
                            std::min(line + static_cast<size_t>(cnt) - 1,
                                     buf.line_count() - 1));
                break;
            case 'Y':
                linewise_op('y', line,
                            std::min(line + static_cast<size_t>(cnt) - 1,
                                     buf.line_count() - 1));
                break;
            case 'J': join_lines(cnt); break;
            case 'p': paste(true, cnt); break;
            case 'P': paste(false, cnt); break;
            case 'u':
                for(int i = 0; i < cnt; i++) buf.undo();
                set_cursor(cursor);
                break;
            case 'r': m_pend = 'r'; break;
            default: break;
        }
        // Unmapped printable keys are swallowed while editing so they
        // can't trigger app shortcuts; read-only views pass them on.
        return !read_only || is_known_command(k);
    }

    static bool is_known_command(char32_t k) {
        switch(k) {
            case 'Y': case 'i': case 'a': case 'I': case 'A': case 'o':
            case 'O': case 'x': case 'X': case 'D': case 'C': case 's':
            case 'S': case 'J': case 'p': case 'P': case 'u': case 'r':
                return true;
            default: return false;
        }
    }

    // ----- Insert mode -----------------------------------------------------

    bool handle_insert(const input::Event& e) {
        const bool ctrl = e.mods.has(input::Mod::Ctrl);
        const bool alt  = e.mods.has(input::Mod::Alt);
        const size_t line = cur_line();
        const size_t ls = buf.line_start(line), le = buf.line_end(line);

        switch(e.code) {
            case input::Key::Escape: leave_insert(); return true;
            case input::Key::Left:
                buf.seal();
                set_cursor(ctrl ? word_back(cursor) : buf.prev_char(cursor));
                return true;
            case input::Key::Right:
                buf.seal();
                set_cursor(ctrl ? word_fwd(cursor) : buf.next_char(cursor));
                return true;
            case input::Key::Up:
            case input::Key::Down: {
                buf.seal();
                size_t last = buf.line_count() - 1;
                size_t target = e.code == input::Key::Down
                                    ? std::min(line + 1, last)
                                    : (line > 0 ? line - 1 : 0);
                cursor = at_want_col(target);
                clamp();
                return true;
            }
            case input::Key::Home: buf.seal(); set_cursor(ls); return true;
            case input::Key::End: buf.seal(); set_cursor(le); return true;
            case input::Key::Enter: {
                buf.seal();
                std::string indent =
                    auto_indent ? indent_of(line, cursor) : std::string{};
                insert_text("\n" + indent);
                buf.seal();
                return true;
            }
            case input::Key::Tab: {
                if(ctrl || alt) return false;
                if(e.mods.has(input::Mod::Shift)) {
                    size_t removed = dedent_line(line);
                    cursor = cursor >= ls + removed ? cursor - removed : ls;
                    m_want_col = -1;
                    return true;
                }
                if(expand_tab) {
                    int col = col_of(buf.line_text(line), cursor - ls);
                    insert_text(std::string(static_cast<size_t>(tw() - col % tw()), ' '));
                } else {
                    insert_text("\t");
                }
                return true;
            }
            case input::Key::Backspace: {
                if(cursor == 0) return true;
                size_t start = buf.prev_char(cursor);
                if(ctrl) start = cursor == ls ? start : std::max(word_back(cursor), ls);
                buf.erase(start, cursor - start);
                cursor     = start;
                m_want_col = -1;
                return true;
            }
            case input::Key::Delete: {
                if(cursor >= buf.size()) return true;
                size_t end = buf.next_char(cursor);
                if(ctrl && cursor < le) end = std::min(word_fwd(cursor), le);
                buf.erase(cursor, end - cursor);
                m_want_col = -1;
                return true;
            }
            default: break;
        }
        if(ctrl || alt) return false;
        if(e.code >= 32 && e.code < input::KeyBackSpace) {
            insert_text(util::encode_utf8(e.code));
            return true;
        }
        return false;
    }

    // ----- state ---------------------------------------------------------------

    int         m_want_col{-1};  // sticky display column for j/k/Up/Down
    int         m_count{0};
    char32_t    m_op{0};         // pending operator: d c y > <
    char32_t    m_pend{0};       // pending prefix: g or r
    std::string m_reg;
    bool        m_reg_linewise{false};

    // Frame-local storage that DrawText views point into.
    mutable std::vector<std::string> m_rows;
    mutable std::string              m_cursor_cell;
};

}  // namespace trux::component
