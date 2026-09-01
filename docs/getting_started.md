# Getting started

This walks through the pieces trux gives you — terminal, renderer, layout,
components, input — by building up
[`examples/target_api.cpp`](examples/target_api.cpp) piece by piece. By the end
you'll have enough of the mental model to read the other examples
(`pane_split.cpp`, `keymap_example.cpp`, `example.cpp`) on your own.

trux is not a widget framework with an owning tree and a lifecycle — it's a
compositor. Every frame, you describe what should be on screen by pushing
component data into a `Renderer`; the renderer resolves, diffs, and presents it.
Nothing owns your components but you.

## Prerequisites

- **GCC 14+** (needs `<print>` and `<expected>` from libstdc++; GCC 13 doesn't
  have them). Clang's minimum supported version hasn't been verified yet — the
  repo's `CMakeUserPresets.json` targets `clang++`, but treat GCC 14 as the
  known-good baseline.
- CMake ≥ 3.14, Ninja.

```sh
cmake -B build -G Ninja
cmake --build build
./build/target_api
```

## 1. The init cycle

Every trux program starts the same way: create a `Terminal`, use its size to
create a `Renderer` and a root `layout::Region`, then bring the terminal into
raw mode.

```cpp
#include <trux/core.hpp>

using namespace trux;

int main() {
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());
    auto root     = layout::init(terminal.size());

    if(auto result = terminal.init(); !result) return 1;
    if(auto result = terminal.enable_raw_mode(); !result) return 1;
}
```

`terminal.init()` and `enable_raw_mode()` both return
`std::expected<void, std::string>` — check them and bail on failure, as above.
`Terminal` owns cleanup via RAII, so there's no matching "shutdown" call you
need to remember in the common case (`shutdown()` exists if you need to trigger
it early).

## 2. Layout: regions and splits

`layout::init(size)` gives you one `Region` covering the whole terminal. A
`Region` doesn't own anything and isn't a widget — it's just a rectangle you can
split and hand to the renderer.

```cpp
auto& split    = root.h_split(20);      // horizontal split, 20% / 80%
auto& subsplit = split[1].v_split(50);  // the 80% side, split again 50/50
```

`h_split`/`v_split` take a percentage and return a `Split`, which behaves like a
2-element container (`split[0]`, `split[1]`, or structured bindings:
`auto& [a, b] = root.h_split(20);`). There are also `_fixed` (cell count instead
of percent) and `_shared` variants for splits whose regions need to be
tracked/reused elsewhere.

## 3. Components are values, not widgets

Components in `trux::component` (`List`, `Menu`, `Dropdown`, `Label`,
`Checkbox`, `Dialog`, `Paragraph`, `Split`, `TextInput`) are plain structs. Most
take references to your own state — you own the data, the component just knows
how to draw and mutate it:

```cpp
std::vector<std::string> dir_names = {"😀", "╭", "界"};
int menu_selection1 = 0;
int menu_offset1    = 0;

auto widget1 = component::Menu(dir_names, menu_selection1, menu_offset1)
             | component::BorderRounded;
```

`Menu`'s constructor is `Menu(items, selected, scroll_offset)` — the last two
are `int&`, so `menu_selection1`/`menu_offset1` must outlive the component and
the component mutates them directly as the user navigates.

`| component::BorderRounded` sets a display flag at construction time. Flags can
also be set after the fact:

```cpp
widget2 |= component::BorderRounded;
```

Both spellings go through the same `ComponentFlags` bitset
(`component_flags.hpp`); which one you use is purely a matter of whether you
have the component variable yet. Flags can be combined with `|`
(`component::BorderRounded | component::Bold`).

Other constructors follow the same "you own the state" shape:

```cpp
component::Checkbox(flag_names, flag_checked, checkbox_cursor, checkbox_offset)
component::TextInput{rename_value, rename_cursor}
component::List(items, scroll_offset)
```

## 4. Dialog: wrapping another component

`Dialog<T>` is a template over whatever component it should show inside a
centered, bordered box:

```cpp
bool                 dialog_open   = false;
std::string          rename_value  = "main.cpp";
int                  rename_cursor = rename_value.size();
component::TextInput input_modal{rename_value, rename_cursor};

component::Dialog dialog{
    .title   = "Rename",
    .open    = &dialog_open,
    .content = input_modal,
};
dialog |= component::BorderRounded;
```

`open` is a `bool*` the dialog itself writes to (it closes on Escape via its
`handle()`), so `dialog_open` needs to live as long as the dialog does.

## 5. The frame loop

Everything above is setup that happens once. The loop itself is small:

```cpp
while(!terminal.should_quit()) {
    renderer.resize(root);
    renderer.begin_draw();
    {
        renderer.push(widget1, split[0]);
        renderer.push(widget2, subsplit[0]);
        renderer.push(widget3, subsplit[1]);
        if(dialog_open) renderer.push(dialog, renderer.region(), true);
    }
    renderer.end_draw();

    terminal.present(renderer);

    if(auto event = input.poll(terminal)) {
        using namespace trux::input;
        if(event.kind == EventKind::Quit) break;
        switch(event) {
            case 'q': terminal.request_quit(); break;
            case 'r': dialog_open = !dialog_open; break;
            default: break;
        }
    }
}
```

- `renderer.resize(root)` — call every frame; it's what propagates a
  `SIGWINCH`-driven terminal resize down through the split tree.
- `begin_draw()` / `push(...)` / `end_draw()` — `push` is the equivalent of
  `frame.render_widget` in other immediate-mode frameworks. Push each component
  against the region it should occupy. The third `push` argument (`modal`) is
  `true` for the dialog, pushed against `renderer.region()` (the whole root
  area) rather than a split slot — that's what makes it draw over everything
  else and eat input first while open.
- `terminal.present(renderer)` — diffs the back buffer against the front buffer
  and writes only what changed.
- `input.poll(terminal)` — returns a `bool`-convertible `input::Event`
  (`explicit operator bool() const { return valid; }`), so
  `if(auto event = input.poll(terminal))` skips the body entirely on an empty
  poll. `switch(event)` works because `Event` has `operator char32_t()`,
  encoding the key plus any modifiers into one value — see
  [`KEYMAP.md`](KEYMAP.md) if you need to match modifier combinations, not just
  plain characters.

Components that are focused/visible get first look at each event through
`renderer.push`'s internal dispatch (component `handle()` methods); the
`switch(event)` in your own loop only sees what nothing else consumed — same
pattern `KEYMAP.md` builds on for chorded bindings.

## 6. Where to go next

- **`examples/pane_split.cpp`** — `component::Split` for runtime pane
  splitting/closing (`split_v`, `close_active`), instead of a fixed layout
  decided once at startup.
- **`examples/keymap_example.cpp`** and **[`KEYMAP.md`](KEYMAP.md)** — multi-key
  chords (`gg`, `Ctrl+K Ctrl+S`) layered on top of the same `switch(event)`
  fallthrough shown above.
- **`examples/example.cpp`** — a fuller gallery touching most component types at
  once.
- **[`README.md`](README.md)** — current implementation status
  (`What's implemented`) and project layout.
- **[`TODO.md`](TODO.md)** — the source of truth for what's done, partial, or
  stubbed. Worth checking before relying on anything not covered above.
