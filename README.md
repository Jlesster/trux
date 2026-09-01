<div align="center">

# trux

**A lightweight, immediate-mode terminal composition and rendering engine for
C++23.**

Not a widget framework — a compositor. Built in the spirit of SDL, Ratatui, and
Wayland.

[![Language](https://img.shields.io/badge/language-C%2B%2B23-00599C.svg?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-064F8C.svg?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/tests-CTest-brightgreen.svg?style=flat-square&logo=cmake&logoColor=white)](tests)
[![Platform](https://img.shields.io/badge/platform-Linux-informational.svg?style=flat-square&logo=linux&logoColor=white)](#building)
[![Status](https://img.shields.io/badge/status-pre--1.0-orange.svg?style=flat-square)](TODO.md)
[![License](https://img.shields.io/badge/license-MIT-purple.svg?style=flat-square)](#license)

</div>

---

## What is trux?

Trux describes a frame by pushing component data into a renderer. The renderer
composes, resolves, buffers, and presents the result to the terminal — no widget
ownership trees, no inheritance, no hidden lifecycle.

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
auto widget2 = component::Dropdown(drop_options, selection2, offset2);
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

See [`examples/target_api.cpp`](examples/target_api.cpp) for the full runnable
version.

## What's implemented

The core compositor pipeline — terminal, layout, cell buffer diffing, focus,
resize, and input parsing — is in solid shape. The styling/theming and overlay
layer above it isn't built yet.

| Area                                                                                                                                | Status                                                   |
| ----------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| Terminal abstraction (raw mode, alternate screen, ANSI backend)                                                                     | ✅                                                       |
| Cell buffer + front/back frame diffing                                                                                              | ✅                                                       |
| Layout / region splitting                                                                                                           | ✅                                                       |
| Components — `List`, `Menu`, `Dropdown`, `Label`, `Checkbox`, `Dialog`, `Paragraph`, `Split`, `TextInput`, borders, bitflag styling | ✅                                                       |
| Input protocol — CSI-u/Kitty modifiers, function keys, SGR mouse, bracketed paste                                                   | ✅                                                       |
| Focus manager — tab traversal, mouse click-to-focus, scoped dispatch                                                                | ✅                                                       |
| Resize handling — `SIGWINCH` → resize event → full-redraw invalidation                                                              | ✅                                                       |
| Wide/CJK glyph width accounting                                                                                                     | ✅                                                       |
| Async task spawning (`Executor`) + generic async queue (`Channel<T>`)                                                               | ✅ (Channel not yet wired into the main input-poll loop) |
| Chord/keymap resolution, `Command` type, `Theme`/`StyledSpan`, floating/overlay regions, clipboard                                  | ❌ not started                                           |

Two tests currently fail on a clean build, and — because the test harness has no
per-test isolation — each failure also prevents everything scheduled after it in
the same binary from running, so real test-verified coverage is smaller than
"7/9 pass" suggests. Full detail on all of the above, including exactly what's
confirmed by test vs. confirmed by reading the code, is in [`TODO.md`](TODO.md).

## Building

Requires **GCC 14+** (needs `<print>` and `<expected>` — GCC 13's libstdc++
doesn't have them, confirmed by testing directly), CMake ≥ 3.14, and Ninja.
Note: `CMakeUserPresets.json` in this repo targets `clang++`; the minimum Clang
version needed hasn't been verified yet.

```sh
cmake -B build -G Ninja
cmake --build build
```

Run the example:

```sh
./build/target_api
```

## Testing

```sh
cd build
ctest --output-on-failure
```

Test coverage lives under [`tests/`](tests) — cell buffer, splitting, rendering,
region, input, focus, widgets, event parsing, and async. 7 of 9 binaries
currently pass; see [`TODO.md`](TODO.md) for the two failures and why they hide
more than they show.

## Project layout

```
include/trux/     public headers (component, renderer, layout, terminal, input, style, async, focus)
src/              implementation
examples/         runnable example(s)
tests/            unit tests (CTest)
```

## License

Licensed under MIT

## Author

[Jlesster](https://codeberg.org/Jlesster)
