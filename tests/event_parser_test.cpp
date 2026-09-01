#include "test.hpp"

#include <cassert>
#include <trux/input/event_parser.hpp>

using namespace trux;

namespace {

std::optional<input::Event> feed(input::EventParser& parser,
                                 std::string_view    bytes) {
    std::optional<input::Event> result;
    for(char b : bytes) {
        if(auto e = parser.parse(b)) result = e;
    }
    return result;
}

}  // namespace

void test_plain_ascii() {
    input::EventParser parser;
    auto               e = feed(parser, "a");
    assert(e && e->valid);
    assert(e->code == U'a');
    assert(e->mods == input::Modifiers{});
}

void test_enter_tab_backspace() {
    input::EventParser parser;

    auto enter = feed(parser, "\n");
    assert(enter && enter->code == input::Key::Enter);

    auto tab = feed(parser, "\t");
    assert(tab && tab->code == input::Key::Tab);

    auto bksp = feed(parser, "\x7f");
    assert(bksp && bksp->code == input::Key::Backspace);
}

void test_standalone_escape_and_reprocess() {
    input::EventParser parser;

    auto first = parser.parse('\x1b');
    assert(!first.has_value());
    assert(parser.pending());

    auto second = parser.parse('\x1b');
    assert(second.has_value());
    assert(second->code == input::Key::Escape);

    auto reprocess = parser.take_reprocess();
    assert(reprocess.has_value());
    assert(*reprocess == '\x1b');
}

void test_escape_printable_is_alt_key() {
    input::EventParser parser;

    auto first = parser.parse('\x1b');
    assert(!first.has_value());

    auto second = parser.parse('x');
    assert(second.has_value());
    assert(second->code == static_cast<char32_t>('x'));
    assert(second->mods.has(input::Mod::Alt));
    assert(!parser.pending());
}

void test_resolve_pending_on_bare_escape() {
    input::EventParser parser;

    parser.parse('\x1b');
    assert(parser.pending());

    auto resolved = parser.resolve_pending();
    assert(resolved && resolved->code == input::Key::Escape);
    assert(!parser.pending());
}

void test_arrow_keys_no_modifier() {
    input::EventParser parser;

    auto up = feed(parser, "\x1b[A");
    assert(up && up->code == input::Key::Up);
    assert(up->mods == input::Modifiers{});

    auto down = feed(parser, "\x1b[B");
    assert(down && down->code == input::Key::Down);

    auto right = feed(parser, "\x1b[C");
    assert(right && right->code == input::Key::Right);

    auto left = feed(parser, "\x1b[D");
    assert(left && left->code == input::Key::Left);
}

void test_arrow_key_with_ctrl_modifier() {
    input::EventParser parser;

    // CSI 1;5A -> Up with modifier param 5 (bits=4 -> Ctrl)
    auto e = feed(parser, "\x1b[1;5A");
    assert(e && e->code == input::Key::Up);
    assert(e->mods.has(input::Mod::Ctrl));
    assert(!e->mods.has(input::Mod::Shift));
    assert(!e->mods.has(input::Mod::Alt));
}

void test_tilde_sequences() {
    input::EventParser parser;

    auto del = feed(parser, "\x1b[3~");
    assert(del && del->code == input::Key::Delete);

    auto pgup = feed(parser, "\x1b[5~");
    assert(pgup && pgup->code == input::Key::PageUp);

    auto pgdn = feed(parser, "\x1b[6~");
    assert(pgdn && pgdn->code == input::Key::PageDown);

    auto f5 = feed(parser, "\x1b[15~");
    assert(f5 && f5->code == input::Key::F5);
}

void test_ss3_function_keys() {
    input::EventParser parser;

    auto f1 = feed(parser, "\x1bOP");
    assert(f1 && f1->code == input::Key::F1);

    auto f2 = feed(parser, "\x1bOQ");
    assert(f2 && f2->code == input::Key::F2);

    auto f3 = feed(parser, "\x1bOR");
    assert(f3 && f3->code == input::Key::F3);

    auto f4 = feed(parser, "\x1bOS");
    assert(f4 && f4->code == input::Key::F4);
}

void test_ss3_incomplete_resolves_to_escape() {
    input::EventParser parser;

    parser.parse('\x1b');
    parser.parse('O');
    assert(parser.pending());

    auto resolved = parser.resolve_pending();
    assert(resolved && resolved->code == input::Key::Escape);
    assert(!parser.pending());
}

void test_csi_u_basic_codepoint() {
    input::EventParser parser;

    // Kitty CSI-u: CSI 97u -> unicode codepoint 97 ('a')
    auto e = feed(parser, "\x1b[97u");
    assert(e && e->code == U'a');
    assert(e->mods == input::Modifiers{});
}

void test_csi_u_special_codepoints() {
    input::EventParser parser;

    auto enter = feed(parser, "\x1b[13u");
    assert(enter && enter->code == input::Key::Enter);

    auto bksp = feed(parser, "\x1b[127u");
    assert(bksp && bksp->code == input::Key::Backspace);

    auto esc = feed(parser, "\x1b[27u");
    assert(esc && esc->code == input::Key::Escape);
}

void test_csi_u_with_ctrl_modifier() {
    input::EventParser parser;

    // CSI 105;5u -> 'i' (105) with modifier param 5 -> Ctrl
    auto e = feed(parser, "\x1b[105;5u");
    assert(e && e->code == U'i');
    assert(e->mods.has(input::Mod::Ctrl));
}

void test_csi_u_shift_alt_modifier() {
    input::EventParser parser;

    // modifier param 4 -> bits=3 -> Shift(1) | Alt(2)
    auto e = feed(parser, "\x1b[97;4u");
    assert(e && e->code == U'a');
    assert(e->mods.has(input::Mod::Shift));
    assert(e->mods.has(input::Mod::Alt));
    assert(!e->mods.has(input::Mod::Ctrl));
}

void test_mouse_left_press() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[<0;10;5M");
    assert(e && e->kind == input::EventKind::Mouse);
    assert(e->mouse.button == input::MouseButton::Left);
    assert(e->mouse.kind == input::MouseKind::Press);
    assert(e->mouse.position.x == 9 && e->mouse.position.y == 4);
}

void test_mouse_left_release() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[<0;10;5m");
    assert(e && e->mouse.kind == input::MouseKind::Release);
}

void test_mouse_drag() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[<32;10;5M");
    assert(e && e->mouse.kind == input::MouseKind::Drag);
    assert(e->mouse.button == input::MouseButton::Left);
}

void test_mouse_scroll() {
    input::EventParser parser;
    auto               up = feed(parser, "\x1b[<64;1;1M");
    assert(up && up->mouse.kind == input::MouseKind::ScrollUp);

    auto down = feed(parser, "\x1b[<65;1;1M");
    assert(down && down->mouse.kind == input::MouseKind::ScrollDown);
}

void test_mouse_shift_modifier() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[<4;1;1M");
    assert(e && e->mouse.mods.has(input::Mod::Shift));
}

void test_bracketed_paste_basic() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[200~hello\x1b[201~");
    assert(e && e->kind == input::EventKind::Paste);
    assert(e->paste == "hello");
}

void test_bracketed_paste_with_embedded_escape() {
    input::EventParser parser;
    // pasted content containing a bare ESC that isn't the real terminator
    auto e = feed(parser, "\x1b[200~a\x1bXb\x1b[201~");
    assert(e && e->kind == input::EventKind::Paste);
    assert(e->paste == "a\x1bXb");
}

void test_bracketed_paste_empty() {
    input::EventParser parser;
    auto               e = feed(parser, "\x1b[200~\x1b[201~");
    assert(e && e->paste.empty());
}

int main() {
    test::run("plain_ascii", test_plain_ascii);
    test::run("enter_tab_backspace", test_enter_tab_backspace);
    test::run("standalone_escape_and_reprocess",
              test_standalone_escape_and_reprocess);
    test::run("resolve_pending_on_bare_escape",
              test_resolve_pending_on_bare_escape);
    test::run("escape_printable_is_alt_key", test_escape_printable_is_alt_key);
    test::run("arrow_keys_no_modifier", test_arrow_keys_no_modifier);
    test::run("arrow_key_with_ctrl_modifier",
              test_arrow_key_with_ctrl_modifier);
    test::run("tilde_sequences", test_tilde_sequences);
    test::run("tilde_sequences", test_tilde_sequences);
    test::run("ss3_sequences", test_ss3_function_keys);
    test::run("csi_u_basic_codepoint", test_ss3_incomplete_resolves_to_escape);
    test::run("csi_u_special_codepoints", test_csi_u_special_codepoints);
    test::run("csi_u_with_ctrl_modifier", test_csi_u_with_ctrl_modifier);
    test::run("csi_u_shift_alt_modifier", test_csi_u_shift_alt_modifier);
    test::run("left_mouse_press", test_mouse_left_press);
    test::run("left_mouse_release", test_mouse_left_release);
    test::run("mouse_drag", test_mouse_drag);
    test::run("mouse_scroll", test_mouse_scroll);
    test::run("mouse_shift_modifier", test_mouse_shift_modifier);
    test::run("bracketed_paste_basic", test_bracketed_paste_basic);
    test::run("bracketed_paste_with_embedded_escape",
              test_bracketed_paste_with_embedded_escape);
    test::run("bracketed_paste_empty", test_bracketed_paste_empty);
    return test::summary();
}
