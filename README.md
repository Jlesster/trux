<div align="center">

# trux

**A lightweight, immediate-mode terminal composition and rendering engine for C++23.**

Not a widget framework — a compositor. Built in the spirit of SDL, Ratatui, and Wayland.

[![Language](https://img.shields.io/badge/language-C%2B%2B23-00599C.svg?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-064F8C.svg?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)
[![Dev Shell](https://img.shields.io/badge/dev%20shell-Nix%20flake-5277C3.svg?style=flat-square&logo=nixos&logoColor=white)](https://nixos.org/)
[![Tests](https://img.shields.io/badge/tests-CTest-brightgreen.svg?style=flat-square&logo=cmake&logoColor=white)](tests)
[![Platform](https://img.shields.io/badge/platform-Linux-informational.svg?style=flat-square&logo=linux&logoColor=white)](#building)
[![Status](https://img.shields.io/badge/status-early%20development-orange.svg?style=flat-square)](TODO.md)
[![License](https://img.shields.io/badge/license-MIT-purple.svg?style=flat-square)](#license)

</div>

---

## What is trux?

Trux describes a frame by pushing component data into a renderer. The renderer composes, resolves, buffers, and presents the result to the terminal — no widget ownership trees, no inheritance, no hidden lifecycle.

It's the rendering backend for **Winter**, a planned TUI IDE (IntelliJ × nvim hybrid). Trux itself has no editor-specific concepts baked in — it's the general-purpose compositor Winter will be built on top of.

```
Application → Components → Renderer::push() → Composition Commands
            → Command Resolution → Back Cell Buffer → Frame Diff
            → ANSI Backend → Terminal
```

## Design goals

- Ergonomic, immediate-mode public API — retained/composited internals
- Geometry-first composition (layout is regions, not an owning hierarchy)
- Double-buffered rendering with front/back cell diffing
- Value-oriented component data — no inheritance-based widgets
- No framework-controlled application lifecycle

See [`TRUX_ARCHITECTURE.md`](TRUX_ARCHITECTURE.md) for the full design writeup.

## Example

```cpp
auto terminal = Terminal();
auto renderer = renderer::Renderer(terminal.size());
auto root     = layout::init(terminal.size());

terminal.init();
terminal.enable_raw_mode();

auto& split = root.h_split(20);

auto widget1 = component::Menu(dir_names, offset1, selection1)
             | component::BorderRounded;
auto widget2 = component::Menu(drop_options, selection2, offset2);
widget2 |= component::BorderRounded;

while(running) {
    renderer.resize(root);
    renderer.begin_draw();
    renderer.push(widget1, split[0]);
    renderer.push(widget2, split[1]);
    renderer.end_draw();

    terminal.present(renderer);
}
```

See [`examples/target_api.cpp`](examples/target_api.cpp) for the full runnable version.

## What's implemented

| Area | Status |
|---|---|
| Terminal abstraction (raw mode, alternate screen, ANSI backend) | ✅ |
| Cell buffer + front/back frame diffing | ✅ |
| Layout / region splitting | ✅ |
| Core components — `List`, `Menu`, `Dropdown`, `Label`, borders, bitflag styling | ✅ |
| Stateful input parser (arrows, function keys, CSI-u / Kitty modifiers) | ✅ |
| Basic focus manager | ✅ |
| Async task spawning + resize handling | ✅ |
| Chord/keymap resolution, `TextArea`, floating regions, mouse support | 🚧 |

The 🚧 items, and everything beyond them, are tracked in detail in [`TODO.md`](TODO.md).

## Building

Requires a C++23 compiler, CMake ≥ 3.14, and Ninja. A Nix flake dev shell is provided (`clang`, `cmake`, `ninja`, `clangd`, `gdb`):

```sh
nix develop        # optional, if you use Nix
cmake -B build
cmake --build build
```

Run the example:

```sh
./build/target_api
```

## Testing

```sh
cd build
ctest
```

Test coverage lives under [`tests/`](tests) — cell buffer, splitting, rendering, input, focus, widgets, event parsing, and async.

## Project layout

```
include/trux/     public headers (component, renderer, layout, terminal, input, style, async, focus)
src/              implementation
examples/         runnable example(s)
tests/            unit tests (CTest)
```

## License

No license file is currently included in this repository.

## Author

[Jlesster](https://codeberg.org/Jlesster) — also mirrored on [GitHub](https://github.com/Jlesster/trux)
