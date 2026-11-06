#include "test.hpp"

#include <trux/component/text_area.hpp>
#include <trux/renderer/renderer.hpp>
#include <trux/layout/region.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

using trux::component::TextArea;
using trux::input::Event;
using trux::input::Key;
using trux::input::Mod;
namespace input = trux::input;
using Mode = TextArea::Mode;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if(!(cond)) {                                                        \
            std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__,      \
                         __LINE__, #cond);                                   \
            std::abort();                                                    \
        }                                                                    \
    } while(0)

#define CHECK_TEXT(area, expected)                                           \
    do {                                                                     \
        auto got_ = (area).text();                                           \
        if(got_ != (expected)) {                                             \
            std::fprintf(stderr, "%s:%d: text was \"%s\", wanted \"%s\"\n",  \
                         __FILE__, __LINE__, got_.c_str(),                   \
                         std::string(expected).c_str());                     \
            std::abort();                                                    \
        }                                                                    \
    } while(0)

namespace {

/// Feeds a key script: plain characters are key presses; <Esc> <CR>
/// <Tab> <S-Tab> <BS> <Del> <Left> <Right> <Up> <Down> <C-r> are specials.
/// Returns the handle() result of the last key.
bool keys(TextArea& t, std::string_view script) {
    bool last = false;
    for(size_t i = 0; i < script.size();) {
        Event ev;
        if(script[i] == '<') {
            size_t      end  = script.find('>', i);
            std::string name = std::string(script.substr(i + 1, end - i - 1));
            i                = end + 1;
            if(name == "Esc") ev = Event::key(Key::Escape);
            else if(name == "CR") ev = Event::key(Key::Enter);
            else if(name == "Tab") ev = Event::key(Key::Tab);
            else if(name == "S-Tab")
                ev = Event::key(Key::Tab, input::with(Mod::Shift));
            else if(name == "BS") ev = Event::key(Key::Backspace);
            else if(name == "Del") ev = Event::key(Key::Delete);
            else if(name == "Left") ev = Event::key(Key::Left);
            else if(name == "Right") ev = Event::key(Key::Right);
            else if(name == "Up") ev = Event::key(Key::Up);
            else if(name == "Down") ev = Event::key(Key::Down);
            else if(name == "lt") ev = Event::key('<');
            else if(name == "C-r") ev = Event::key('r', input::with(Mod::Ctrl));
            else CHECK(false);
        } else {
            // Decode one UTF-8 codepoint.
            auto d = trux::util::decode_utf8(script.substr(i));
            CHECK(d.has_value());
            ev = Event::key(d->first);
            i += static_cast<size_t>(d->second);
        }
        last = t.handle(ev);
    }
    return last;
}

}  // namespace

void test_insert_and_escape() {
    TextArea t;
    CHECK(keys(t, "ihello<Esc>"));
    CHECK_TEXT(t, "hello");
    CHECK(t.mode == Mode::Normal);
    CHECK(t.cursor == 4);  // Esc steps back onto the last char
    CHECK(!keys(t, "<Esc>"));  // idle Esc is left for the app
}

void test_tab_routing() {
    TextArea t("x");
    CHECK(!keys(t, "<Tab>"));  // Normal: focus traversal keeps Tab
    keys(t, "i");
    CHECK(keys(t, "<Tab>"));  // Insert: consumed, indents
    CHECK_TEXT(t, "    x");
    CHECK(t.cursor == 4);
    keys(t, "<S-Tab>");
    CHECK_TEXT(t, "x");
    CHECK(t.cursor == 0);
}

void test_tab_stops_and_tabs() {
    TextArea t;
    keys(t, "iab<Tab>c");  // ab + 2 spaces to column 4
    CHECK_TEXT(t, "ab  c");
    TextArea u;
    u.expand_tab = false;
    keys(u, "i<Tab>x");
    CHECK_TEXT(u, "\tx");
    keys(u, "<Esc>0i<S-Tab>");
    CHECK_TEXT(u, "x");
}

void test_enter_autoindent() {
    TextArea t;
    keys(t, "i    foo<CR>bar");
    CHECK_TEXT(t, "    foo\n    bar");
    TextArea u;
    u.auto_indent = false;
    keys(u, "i  foo<CR>bar");
    CHECK_TEXT(u, "  foo\nbar");
}

void test_open_lines() {
    TextArea t("  a\nb");
    keys(t, "ox<Esc>");
    CHECK_TEXT(t, "  a\n  x\nb");
    keys(t, "gg");
    keys(t, "Oy<Esc>");
    CHECK_TEXT(t, "  y\n  a\n  x\nb");
}

void test_delete_and_yank_put() {
    TextArea t("one\ntwo\nthree\nfour");
    keys(t, "dd");
    CHECK_TEXT(t, "two\nthree\nfour");
    keys(t, "jp");
    CHECK_TEXT(t, "two\nthree\none\nfour");
    keys(t, "ggyyP");
    CHECK_TEXT(t, "two\ntwo\nthree\none\nfour");
    keys(t, "Gdd");  // last line removes the preceding newline
    CHECK_TEXT(t, "two\ntwo\nthree\none");
    keys(t, "ggdj");
    CHECK_TEXT(t, "three\none");
    keys(t, "dk");  // already at top: fails, deletes nothing
    CHECK_TEXT(t, "three\none");
    keys(t, "2dd");
    CHECK_TEXT(t, "");
}

void test_char_ops() {
    TextArea t("hello world");
    keys(t, "x");
    CHECK_TEXT(t, "ello world");
    keys(t, "3x");
    CHECK_TEXT(t, "o world");
    keys(t, "wdw");
    CHECK_TEXT(t, "o ");
    keys(t, "0D");
    CHECK_TEXT(t, "");
    t.set_text("foo bar baz");
    keys(t, "cwX<Esc>");
    CHECK_TEXT(t, "X bar baz");
    keys(t, "wde");
    CHECK_TEXT(t, "X  baz");
    keys(t, "$x");
    CHECK_TEXT(t, "X  ba");
    keys(t, "0d$");
    CHECK_TEXT(t, "");
}

void test_dw_stops_at_line_end() {
    TextArea t("foo\nbar");
    keys(t, "dw");
    CHECK_TEXT(t, "\nbar");
}

void test_cc_keeps_indent() {
    TextArea t("  foo\nbar");
    keys(t, "ccx<Esc>");
    CHECK_TEXT(t, "  x\nbar");
}

void test_replace_and_join() {
    TextArea t("abc\n   def");
    keys(t, "rX");
    CHECK_TEXT(t, "Xbc\n   def");
    keys(t, "J");
    CHECK_TEXT(t, "Xbc def");
}

void test_indent_operators() {
    TextArea t("a\n\nb");
    keys(t, "3>>");  // empty lines are skipped
    CHECK_TEXT(t, "    a\n\n    b");
    keys(t, "gg<lt><lt>");
    CHECK_TEXT(t, "a\n\n    b");
    keys(t, ">j");
    CHECK_TEXT(t, "    a\n\n    b");
}

void test_undo_grouping() {
    TextArea t("abc");
    keys(t, "Ahello<CR>world<Esc>");
    CHECK_TEXT(t, "abchello\nworld");
    keys(t, "u");  // Enter seals: "world" is its own step
    CHECK_TEXT(t, "abchello\n");
    keys(t, "u");  // ...and so is the newline
    CHECK_TEXT(t, "abchello");
    keys(t, "u");
    CHECK_TEXT(t, "abc");
    keys(t, "<C-r>");
    CHECK_TEXT(t, "abchello");
    keys(t, "u");
    keys(t, "ddu");
    CHECK_TEXT(t, "abc");
    keys(t, "xxu");
    CHECK_TEXT(t, "bc");
    keys(t, "u");
    CHECK_TEXT(t, "abc");
}

void test_navigation() {
    TextArea t("abc\n\nabcdef\nxy");
    keys(t, "$");
    CHECK(t.col() == 2);
    keys(t, "j");  // empty line
    CHECK(t.row() == 1 && t.col() == 0);
    keys(t, "j");  // $ sticks to line ends across the empty line
    CHECK(t.row() == 2 && t.col() == 5);
    keys(t, "G");
    CHECK(t.row() == 3);
    keys(t, "gg");
    CHECK(t.row() == 0 && t.col() == 0);
    keys(t, "2G");
    CHECK(t.row() == 1);
    keys(t, "3j");
    CHECK(t.row() == 3);  // clamped, not rejected
    keys(t, "$k");
    CHECK(t.row() == 2 && t.col() == 5);
}

void test_word_motions() {
    TextArea t("foo, bar_baz  qux");
    keys(t, "w");
    CHECK(t.cursor == 3);  // punctuation is its own word
    keys(t, "w");
    CHECK(t.cursor == 5);
    keys(t, "e");
    CHECK(t.cursor == 11);
    keys(t, "b");
    CHECK(t.cursor == 5);
    keys(t, "2w");
    CHECK(t.cursor == 16);  // end of text clamps onto the last char
}

void test_utf8_editing() {
    TextArea t("日本語");
    keys(t, "x");
    CHECK_TEXT(t, "本語");
    keys(t, "$i<BS>");
    CHECK_TEXT(t, "語");
    keys(t, "<Esc>");
    keys(t, "a😀é<Esc>");
    CHECK_TEXT(t, "語😀é");
}

void test_insert_arrows_and_backspace_join() {
    TextArea t("ab\ncd");
    keys(t, "ji<BS>");
    CHECK_TEXT(t, "abcd");
    keys(t, "<Left><Del>");
    CHECK_TEXT(t, "acd");
    keys(t, "<Down>");  // last line: stays put
    CHECK(t.cursor == 1);
}

void test_paste() {
    TextArea t;
    CHECK(!t.handle(Event::from_paste("x")));  // Normal mode ignores it
    keys(t, "i");
    CHECK(t.handle(Event::from_paste("a\r\nb\rc\x01\td")));
    CHECK_TEXT(t, "a\nb\nc\td");
}

void test_read_only() {
    TextArea t("abc\ndef");
    t.read_only = true;
    keys(t, "ixdd");
    CHECK_TEXT(t, "abc\ndef");
    CHECK(t.mode == Mode::Normal);
    keys(t, "jl");
    CHECK(t.row() == 1 && t.col() == 1);
    keys(t, "yyP");  // yanking is allowed, putting is not
    CHECK_TEXT(t, "abc\ndef");
    CHECK(!keys(t, "q"));  // unmapped keys pass through
}

void test_normal_swallows_unmapped() {
    TextArea t("abc");
    CHECK(keys(t, "q"));
    CHECK(keys(t, "Z"));
    CHECK(!keys(t, "<CR>"));
}

void test_cursor_clamps_to_text() {
    TextArea t("ab");
    t.cursor = 99;
    keys(t, "x");
    CHECK_TEXT(t, "a");
    t.cursor = 1;  // past the last char in Normal mode
    keys(t, "x");
    CHECK_TEXT(t, "");
}

void test_build() {
    namespace r = trux::renderer;
    TextArea t("ab\tc\n日本\nlast");
    t.show_cursor = true;
    t.cursor      = 3;  // 'c'... actually the tab is byte 2, c is byte 3
    r::DrawCommandBuffer cmd;
    t.build(trux::layout::Region({0, 0}, {10, 2}), cmd);
    const auto& c = cmd.commands();
    // two rows + cursor cell
    CHECK(c.size() == 3);
    auto* row0 = std::get_if<r::DrawText>(&c[0]);
    auto* row1 = std::get_if<r::DrawText>(&c[1]);
    auto* cur  = std::get_if<r::DrawText>(&c[2]);
    CHECK(row0 && row1 && cur);
    CHECK(row0->text == "ab  c");   // tab expanded to column 4
    CHECK(row1->text == "日本");
    CHECK(cur->text == "c" && cur->position.x == 4 && cur->position.y == 0);

    // Scrolls vertically to keep the cursor in view.
    t.cursor = 12;  // somewhere on "last"
    cmd = {};
    t.build(trux::layout::Region({0, 0}, {10, 2}), cmd);
    CHECK(t.m_top == 1);
    // Scrolls horizontally on a narrow view.
    TextArea u("0123456789abcdef");
    u.cursor = 15;
    r::DrawCommandBuffer cmd2;
    u.build(trux::layout::Region({0, 0}, {5, 1}), cmd2);
    auto* vis = std::get_if<r::DrawText>(&cmd2.commands()[0]);
    CHECK(vis && vis->text == "bcdef");
    CHECK(u.m_left == 11);
    // Cursor at end-of-line draws a space cell.
    TextArea v("ab");
    v.mode   = Mode::Insert;
    v.cursor = 2;
    r::DrawCommandBuffer cmd3;
    v.build(trux::layout::Region({0, 0}, {10, 1}), cmd3);
    auto* last = std::get_if<r::DrawText>(&cmd3.commands().back());
    CHECK(last && last->text == " " && last->position.x == 2);
}

void test_renderer_tab_routing() {
    // Insert mode: the renderer hands Tab to the editor. Normal mode:
    // it falls back to focus traversal, as for any other component.
    trux::renderer::Renderer r({40, 12});
    TextArea a, b;
    a.mode = Mode::Insert;
    auto frame = [&] {
        r.begin_draw();
        r.push(a, trux::layout::Region{{0, 0}, {20, 5}});
        r.push(b, trux::layout::Region{{0, 5}, {20, 5}});
        r.end_draw();
    };
    frame();
    CHECK(r.dispatch(Event::key(Key::Tab)));
    CHECK_TEXT(a, "    ");
    CHECK_TEXT(b, "");

    CHECK(r.dispatch(Event::key(Key::Escape)));  // back to Normal
    CHECK(a.mode == Mode::Normal);
    frame();
    CHECK(r.dispatch(Event::key(Key::Tab)));  // now moves focus to b
    CHECK_TEXT(a, "    ");
    CHECK(r.dispatch(Event::key('i')));
    CHECK(b.mode == Mode::Insert && a.mode == Mode::Normal);

    // Shift-Tab dedents in Insert mode and does not move focus.
    frame();
    CHECK(r.dispatch(Event::key(Key::Tab)));
    CHECK_TEXT(b, "    ");
    CHECK(r.dispatch(Event::key(Key::Tab, input::with(Mod::Shift))));
    CHECK_TEXT(b, "");
}

int main() {
    test::run("insert_and_escape", test_insert_and_escape);
    test::run("tab_routing", test_tab_routing);
    test::run("tab_stops_and_tabs", test_tab_stops_and_tabs);
    test::run("enter_autoindent", test_enter_autoindent);
    test::run("open_lines", test_open_lines);
    test::run("delete_and_yank_put", test_delete_and_yank_put);
    test::run("char_ops", test_char_ops);
    test::run("dw_stops_at_line_end", test_dw_stops_at_line_end);
    test::run("cc_keeps_indent", test_cc_keeps_indent);
    test::run("replace_and_join", test_replace_and_join);
    test::run("indent_operators", test_indent_operators);
    test::run("undo_grouping", test_undo_grouping);
    test::run("navigation", test_navigation);
    test::run("word_motions", test_word_motions);
    test::run("utf8_editing", test_utf8_editing);
    test::run("insert_arrows_and_backspace_join", test_insert_arrows_and_backspace_join);
    test::run("paste", test_paste);
    test::run("read_only", test_read_only);
    test::run("normal_swallows_unmapped", test_normal_swallows_unmapped);
    test::run("cursor_clamps_to_text", test_cursor_clamps_to_text);
    test::run("build", test_build);
    test::run("renderer_tab_routing", test_renderer_tab_routing);
    return test::summary();
}
