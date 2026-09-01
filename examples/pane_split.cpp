#include "trux/component/component.hpp"
#include "trux/component/split.hpp"

#include <trux/core.hpp>
using namespace trux;
int main() {
    auto         terminal = Terminal();
    auto         renderer = renderer::Renderer(terminal.size());
    auto         root     = layout::init(terminal.size());
    input::Input input;

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;

    std::vector<std::string> dir_names    = {"😀", "╭", "界"};
    std::vector<std::string> drop_options = {"yes", "no", "maybe"};

    auto& split = root.h_split(20);

    int menu_selection1 = 0;
    int menu_selection2 = 0;
    int menu_offset1    = 0;
    int menu_offset2    = 0;

    component::Split panes{
        component::Menu(dir_names, menu_selection1, menu_offset1) |
        component::BorderRounded};
    auto widget2 = component::Menu(drop_options, menu_selection2, menu_offset2);

    bool                 dialog_open   = false;
    std::string          rename_value  = "main.cpp";
    int                  rename_cursor = rename_value.size();
    component::TextInput input_modal{rename_value, rename_cursor};
    component::Dialog    dialog{
           .title   = "Rename",
           .open    = &dialog_open,
           .content = input_modal,
    };
    widget2 |= component::BorderRounded;
    dialog |= component::BorderRounded;
    std::vector<std::string> log_lines  = {"pane tree ready"};
    int                      log_scroll = 0;
    while(!terminal.should_quit()) {
        renderer.resize(root);
        renderer.begin_draw();
        {
            renderer.push(panes, split[0]);
            renderer.push(widget2, split[1]);
            if(dialog_open)
                renderer.push(dialog, renderer.region(), /*modal=*/true);
        }
        renderer.end_draw();
        terminal.present(renderer);
        if(auto event = input.poll(terminal)) {
            if(event.kind == input::EventKind::Quit) break;
            switch(event) {
                case 'q':
                    terminal.request_quit();
                    break;
                case 'r':
                    dialog_open = !dialog_open;
                    break;
                case 'p':
                    panes.split_v(component::List(log_lines, log_scroll) |
                                  component::BorderRounded);
                    break;
                case 'x':
                    panes.close_active();
                    break;
                default:
                    break;
            }
        }
    }
}
