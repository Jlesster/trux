#include "trux/component/component.hpp"
#include "trux/component/label.hpp"
#include "trux/component/menu.hpp"
#include "trux/input/keymap.hpp"

#include <algorithm>
#include <trux/core.hpp>

using namespace trux;

// Demonstrates trux::input::Keymap<T> -- multi-key chord resolution sitting
// between raw input::Event and application logic. Bindings shown:
//
//   g g          vim-style: jump to the top of the list
//   G            single-key chord: jump to the bottom
//   d d          vim-style: delete the highlighted item
//   Ctrl+K Ctrl+S   emacs-style leader chord: "save"
//
// j/k/Up/Down/Enter are still handled by Menu::handle() as usual -- the
// keymap only ever sees events Menu didn't consume, exactly like the
// hand-written `switch(event)` blocks in the other examples. Chords aren't
// a replacement for that routing, just another consumer of it.
enum class Cmd { GotoTop, GotoBottom, DeleteItem, Save };

int main() {
    auto         terminal = Terminal();
    auto         renderer = renderer::Renderer(terminal.size());
    auto         root     = layout::init(terminal.size());
    input::Input input;

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    // --- keymap -----------------------------------------------------------
    input::Keymap<Cmd> keymap;
    // default timeout; set explicitly here for clarity
    keymap.set_timeout(std::chrono::milliseconds(600));
    keymap.bind({'g', 'g'}, Cmd::GotoTop);
    keymap.bind({'G'}, Cmd::GotoBottom);
    keymap.bind({'d', 'd'}, Cmd::DeleteItem);
    // Encode Ctrl+K / Ctrl+S the same way Event::operator char32_t() does
    // (code | mods << 24), so the bound chord matches what feed() computes
    // from a real keypress -- see KEYMAP.md's "encoding modifiers" section.
    keymap.bind({'k' | input::Mod::Ctrl, 's' | input::Mod::Ctrl}, Cmd::Save);

    // check_timeout() has to be polled on a clock, not just on key events --
    // a dangling "g" with nothing typed after it must still resolve (or
    // expire) even if the user never presses another key. want_tick makes
    // poll() return Event::tick() every ~100ms whenever nothing else is
    // pending, which is what drives that check below.
    input.set_want_tick(true);

    // --- state --------------------------------------------------------
    std::vector<std::string> items = {
        "src/terminal/terminal.cpp",
        "src/renderer/renderer.cpp",
        "src/layout/region.cpp",
        "src/input/event_parser.cpp",
        "src/input/input.cpp",
    };
    int selected = 0;
    int offset   = 0;

    std::string status =
        "gg: top   G: bottom   dd: delete   ^K^S: save   q: quit";

    // --- widgets ------------------------------------------------------
    auto file_menu =
        component::Menu(items, selected, offset) | component::BorderRounded;
    component::Label status_label{status};

    auto& rows = root.h_split(85);  // rows[0] menu, rows[1] status line

    while(!terminal.should_quit()) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            renderer.push(file_menu, rows[0]);
            status_label.text = status;
            renderer.push(status_label, rows[1]);
        }
        renderer.end_draw();
        terminal.present(renderer);

        auto event = input.poll(terminal);
        if(!event) continue;
        if(event.kind == input::EventKind::Quit) break;

        Cmd                resolved{};
        input::ChordResult result = input::ChordResult::NoMatch;

        if(event.kind == input::EventKind::Tick) {
            // No key arrived this tick -- only matters if a chord is
            // hanging (e.g. a lone "g" waiting on a possible second "g").
            result = keymap.check_timeout(resolved);
        } else if(!event.consumed) {
            result = keymap.feed(event, resolved);
        }

        if(result == input::ChordResult::Pending) {
            status = "-- pending chord --";
        } else if(result == input::ChordResult::Matched) {
            switch(resolved) {
                case Cmd::GotoTop:
                    selected = 0;
                    status   = "gg -> jumped to top";
                    break;
                case Cmd::GotoBottom:
                    selected = static_cast<int>(items.size()) - 1;
                    status   = "G -> jumped to bottom";
                    break;
                case Cmd::DeleteItem:
                    if(!items.empty()) {
                        items.erase(items.begin() + selected);
                        selected = std::clamp(
                            selected, 0, static_cast<int>(items.size()) - 1);
                    }
                    status = "dd -> deleted item";
                    break;
                case Cmd::Save:
                    status = "^K^S -> saved (pretend)";
                    break;
            }
        } else if(event.kind == input::EventKind::Key && !event.consumed) {
            // NoMatch here means feed() has definitively decided this key
            // (possibly after silently reprocessing an abandoned chord
            // prefix -- see KEYMAP.md's "mismatch recovery" section) isn't
            // starting or continuing any bound chord. Global, non-chorded
            // bindings live here, same as the plain switch(event) in the
            // other examples.
            switch(event) {
                case 'q':
                    terminal.request_quit();
                    break;
                default:
                    break;
            }
        }
    }
}
