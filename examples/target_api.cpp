#include "trux/component/menu.hpp"
#include "trux/input/input.hpp"
#include "trux/input/key.hpp"

#include <trux/component/component.hpp>
#include <trux/component/dropdown.hpp>
#include <trux/layout/layout.hpp>
#include <trux/terminal/terminal.hpp>

using namespace trux;

// Example api
int main() {
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root     = layout::init(terminal.size());

    input::Input input;

    bool running = true;

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    // vector examples for lists
    std::vector<std::string> dir_names    = {"😀", "╭", "界"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};

    // splitting with either which we already have
    auto& split = root.h_split(20);
    // auto [split1, split2] = root.h_split(20);

    // menu selection
    int menu_selection1 = 0;
    int menu_selection2 = 0;
    int menu_offset1    = 0;
    int menu_offset2    = 0;

    // creating widgets as variables and giving attribs
    auto widget1 = component::Menu(dir_names, menu_offset1, menu_selection1) |
                   component::BorderRounded;
    auto widget2 = component::Menu(drop_options, menu_selection2, menu_offset2);

    // giving widgets attribs after creation
    widget2 |= component::BorderRounded;

    // beginning and ending drawing sdl like
    while(running) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            // the equivalent of frame.render_widget
            renderer.push(widget1, split[0]);
            renderer.push(widget2, split[1]);
            // OR
            // renderer.push(widget1, split1);
            // renderer.push(widget1, split2);
        }
        renderer.end_draw();

        // presenting the final buffer to the terminal
        terminal.present(renderer);

        if(auto event = input.poll(terminal)) {
            switch(event) {
                case 'q':
                    running = false;
                    break;
                case input::Key::Escape:
                    running = false;
                    break;
                default:
                    break;
            }
        }
    }
}
