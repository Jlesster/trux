#include "trux/core.hpp"

using namespace trux;
int main() {
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root = layout::init(terminal.size());
    input::Input input;

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    std::vector<std::string> dir_names = {"😀", "╭", "界"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};

    auto& split = root.h_split(20);

    component::Split panes{component::Menu(dir_names) |
                           component::BorderRounded};
    auto widget2 = component::Menu(drop_options);

    component::Dialog dialog{.title = "Rename"};
    dialog.input.set_value("main.cpp");
    widget2 |= component::BorderRounded;
    dialog |= component::BorderRounded;
    std::vector<std::string> log_lines = {"pane tree ready"};
    while(!terminal.should_quit()) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            renderer.push(panes, split[0]);
            renderer.push(widget2, split[1]);
            if(dialog.open)
                renderer.push(dialog, renderer.region(), /*modal=*/true);
        }
        renderer.end_draw();
        terminal.present(renderer);
        if(auto event = input.poll(terminal)) {
            if(event.kind == input::EventKind::Quit) break;
            switch(event) {
                case 'q': terminal.request_quit(); break;
                case 'r': dialog.open = !dialog.open; break;
                case 'p':
                    panes.split_v(component::List(log_lines) |
                                  component::BorderRounded);
                    break;
                case 'x': panes.close_active(); break;
                default: break;
            }
        }
    }
}
