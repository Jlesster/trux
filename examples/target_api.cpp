#include "trux/component/checkbox.hpp"
#include "trux/component/component.hpp"

#include <trux/core.hpp>

using namespace trux;

// Example api
int main() {
    // this is the entire init cycle,
    // create terminal, terminal size then is used to
    // create the renderer and root rect
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root = layout::init(terminal.size());

    // create an input object for polling and to plug into
    // a switch statement handling key and modifier cases
    input::Input input;

    // the below enable and
    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    // vector examples for lists
    std::vector<std::string> dir_names = {"😀", "╭", "界"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};
    std::vector<std::string> flag_names = {
        "-Wall", "-Werror", "-lpthread", "-O3"};
    std::vector<bool> flag_checked(flag_names.size(), false);

    // split vairables can be assigned either way
    auto& split = root.h_split(20);
    auto& subsplit = split[1].v_split(50);
    // auto& [split1, split2] = root.h_split(20);

    // creating widgets as variables and giving attribs. selection, cursor
    // and scroll offset live inside the widgets themselves
    // (widget1.selection, widget3.cursor, ...)
    auto widget2 = component::Menu(drop_options);
    auto widget1 = component::Menu(dir_names) | component::BorderRounded;

    auto widget3 = component::Checkbox(flag_names, flag_checked) |
                   component::BorderRounded;

    // the dialog embeds its own text field: dialog.input.value
    component::Dialog dialog{.title = "Rename"};
    dialog.input.set_value("main.cpp");

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
            if(dialog.open) renderer.push(dialog, renderer.region(), true);
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
                case 'q': terminal.request_quit(); break;
                case 'r': dialog.open = !dialog.open; break;

                default: break;
            }
        }
    }
}
