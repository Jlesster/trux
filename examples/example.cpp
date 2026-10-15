#include "trux/component/checkbox.hpp"
#include "trux/component/component.hpp"
#include "trux/component/dropdown.hpp"
#include "trux/component/paragraph.hpp"

#include <trux/core.hpp>

using namespace trux;

// Showcases every component currently in trux, laid out roughly like the
// FTXUI compiler-config demo: a left column for single/multi-select and a
// right column for freeform/list/paragraph content, plus a modal dialog.
int main() {
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root = layout::init(terminal.size());
    input::Input input;

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;
    renderer.set_border_colors({250, 200, 80, 255});
    style::Color::ActiveBorderColor({160, 0, 160, 255});

    // --- layout -------------------------------------------------------
    // top strip: title Label. body: left column (Menu/Checkbox) vs
    // right column (Dropdown/TextInput/List/Paragraph).
    auto& rows = root.h_split(15);     // rows[0] top strip, rows[1] body
    auto& cols = rows[1].v_split(35);  // cols[0] left, cols[1] right

    auto& left_rows = cols[0].h_split(50);   // compiler / flags
    auto& right_rows = cols[1].h_split(25);  // build type strip

    auto& right_mid = right_rows[1].h_split(33);  // executable name
    auto& right_low = right_mid[1].h_split(50);   // log list / notes

    // --- state ----------------------------------------------------------
    std::vector<std::string> compilers = {"gcc", "clang", "emcc"};

    std::vector<std::string> flag_names = {
        "-Wall", "-Werror", "-lpthread", "-O3"};
    std::vector<bool> flag_checked = {true, false, false, false};

    std::vector<std::string> build_types = {
        "Debug", "Release", "RelWithDebInfo"};

    std::vector<std::string> build_log = {
        "cloned source", "configured", "ready to build"};

    // --- widgets ----------------------------------------------------------
    component::Label title{"trux component gallery"};

    auto compiler_menu = component::Menu(compilers) | component::BorderRounded;

    auto flag_checkbox = component::Checkbox(flag_names, flag_checked) |
                         component::BorderRounded;

    auto build_dropdown =
        component::Dropdown(build_types) | component::BorderSingle;

    component::TextInput executable_input{"pkgmgr"};
    executable_input |= component::BorderSingle;

    auto log_list = component::List(build_log) | component::BorderSingle;

    component::Paragraph notes_paragraph{
        "Per-package build configuration. Space toggles a flag, Tab moves "
        "focus between panels, Enter opens the rename dialog."};
    notes_paragraph |= component::BorderSingle;

    // --- modal dialog: rename the executable -------------------------
    component::Dialog dialog{.title = "Rename executable"};
    // the dialog's input is its own copy of the text; push it back into
    // the main field when the user confirms with Enter.
    dialog.on_confirm = [&] { executable_input.set_value(dialog.input.value); };
    dialog |= component::BorderRounded;

    while(!terminal.should_quit()) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            renderer.push(title, rows[0]);

            renderer.push(compiler_menu, left_rows[0]);
            renderer.push(flag_checkbox, left_rows[1]);

            renderer.push(build_dropdown, right_rows[0]);
            renderer.push(executable_input, right_mid[0]);
            renderer.push(log_list, right_low[0]);
            renderer.push(notes_paragraph, right_low[1]);

            if(dialog.open)
                renderer.push(dialog, renderer.region(), /*modal=*/true);
        }
        renderer.end_draw();
        terminal.present(renderer);

        if(auto event = input.poll(terminal)) {
            if(event.kind == input::EventKind::Quit) break;

            // route to whichever component has focus (Tab/Shift+Tab cycle
            // focus internally); only fall through to the global keys
            // below if nothing consumed the event.
            if(event.consumed) continue;

            switch(event) {
                case 'q': terminal.request_quit(); break;
                case input::Key::Enter:
                    dialog.open = true;
                    dialog.input.set_value(executable_input.value);
                    break;
                default: break;
            }
        }
    }
}
