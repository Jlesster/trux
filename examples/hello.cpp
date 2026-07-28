#include <trux/component/component.hpp>
#include <trux/layout/layout.hpp>
#include <trux/terminal/terminal.hpp>

using namespace trux;

// Example api
int main() {
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root     = layout::init(terminal.size());

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    // vector examples for lists
    std::vector<std::string> dir_names;
    std::vector<std::string> drop_options;

    // splitting with either which we already have
    auto split            = root.h_split(20);
    auto [split1, split2] = root.h_split(20);

    // creating widgets as variables and giving attribs
    auto widget1 = component::List(dir_names).BorderRounded;
    auto widget2 = component::Dropdown(drop_options);

    // giving widgets attribs after creation
    widget2.BorderSingle.Italic;

    // beginning and ending drawing sdl like
    renderer.begin_draw();
    {
        // the equivalent of frame.render_widget
        renderer.push(widget1, split[0]);
        renderer.push(widget1, split[1]);
        // OR
        renderer.push(widget1, split1);
        renderer.push(widget1, split2);
    }
    renderer.end_draw();

    // presenting the final buffer to the terminal
    terminal.present(renderer);
}
