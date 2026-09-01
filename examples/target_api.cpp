#include "trux/component/checkbox.hpp"
#include "trux/component/component.hpp"
#include "trux/input/modifiers.hpp"

#include <trux/core.hpp>

using namespace trux;

// Example api
int main() {
    // this is the entire init cycle,
    // create terminal, terminal size then is used to
    // create the renderer and root rect
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root     = layout::init(terminal.size());

    // create an input object for polling and to plug into
    // a switch statement handling key and modifier cases
    input::Input input;

    // the below enable and
    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    // vector examples for lists
    std::vector<std::string> dir_names    = {"😀", "╭", "界"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};
    std::vector<std::string> flag_names   = {
        "-Wall", "-Werror", "-lpthread", "-O3"};
    std::vector<bool> flag_checked(flag_names.size(), false);

    // split vairables can be assigned either way
    auto& split    = root.h_split(20);
    auto& subsplit = split[1].v_split(50);
    // auto& [split1, split2] = root.h_split(20);

    // menu selection
    int menu_selection1 = 0;
    int menu_selection2 = 0;

    // scroll offset
    int menu_offset1 = 0;
    int menu_offset2 = 0;

    int checkbox_cursor = 0;
    int checkbox_offset = 0;

    // creating widgets as variables and giving attribs
    auto widget2 = component::Menu(drop_options, menu_selection2, menu_offset2);
    auto widget1 = component::Menu(dir_names, menu_selection1, menu_offset1) |
                   component::BorderRounded;

    auto widget3 =
        component::Checkbox(
            flag_names, flag_checked, checkbox_cursor, checkbox_offset) |
        component::BorderRounded;

    bool                 dialog_open   = false;
    std::string          rename_value  = "main.cpp";
    int                  rename_cursor = rename_value.size();
    component::TextInput input_modal{rename_value, rename_cursor};

    component::Dialog dialog{
        .title   = "Rename",
        .open    = &dialog_open,
        .content = input_modal,
    };

    // giving widgets attribs after creation
    widget2 |= component::BorderRounded;
    dialog |= component::BorderRounded;

    // beginning and ending drawing sdl like
    // the framework has a built in handle for
    // safely quitting and letting RAII clean up
    while(!terminal.should_quit()) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            // the equivalent of frame.render_widget
            renderer.push(widget1, split[0]);
            renderer.push(widget2, subsplit[0]);
            renderer.push(widget3, subsplit[1]);
            if(dialog_open) renderer.push(dialog, renderer.region(), true);
            // OR
            // renderer.push(widget1, split1);
            // renderer.push(widget1, split2);
        }
        renderer.end_draw();

        // presenting the final buffer to the terminal
        terminal.present(renderer);

        // easy and convenient input polling that
        // handles
        if(auto event = input.poll(terminal)) {
            // all input is handled by the namespace
            using namespace trux::input;
            if(event.kind == EventKind::Quit) break;
            switch(event) {
                case 'q':
                    terminal.request_quit();
                    break;
                case 'r':
                    dialog_open = !dialog_open;
                    break;

                default:
                    break;
            }
        }
    }
}
