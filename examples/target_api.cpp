#include "trux/component/list.hpp"
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
    std::vector<std::string> dir_names    = {"foo", "bar", "baz"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};

    // splitting with either which we already have
    auto split = root.h_split(20);
    // auto [split1, split2] = root.h_split(20);

    // creating widgets as variables and giving attribs
    auto widget1 = component::List(dir_names) | component::BorderRounded;
    auto widget2 = component::Menu(drop_options);

    // giving widgets attribs after creation
    widget2 |= component::BorderSingle | component::Italic;

    // beginning and ending drawing sdl like
    while(running) {
        renderer.begin_draw();
        {
            // the equivalent of frame.render_widget
            renderer.push(widget1, split[0]);
            renderer.push(widget1, split[1]);
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
