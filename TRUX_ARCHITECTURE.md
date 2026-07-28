# Trux Architecture Notes

## Vision

Trux is **not** a widget framework like FTXUI.

Trux is a lightweight terminal composition and rendering engine in the same
spirit as SDL or a graphics API.

Winter (the IDE) will be built **on top of** Trux.

The user describes a frame by pushing component data into a renderer. The
renderer composes, resolves, buffers, and presents the result.

Core philosophy:

- Terminal manages the terminal.
- Layout manages geometry.
- Components describe UI intent.
- Renderer manages composition and rendering.
- Application owns state and lifecycle.

---

# Design Goals

- Ergonomic C++23 API.
- Immediate-mode public API.
- Retained/composited internal architecture.
- Geometry-first composition.
- Double buffered rendering.
- No React-style component trees.
- No inheritance-based widget systems.
- No hidden ownership.
- No framework-controlled lifecycle.
- Value-oriented data composition.

---

# High-Level Architecture

```
Application

    |
    v

Components
(descriptions of UI)

    |
    v

Renderer::push()

    |
    v

Composition Commands

    |
    v

Command Resolution

    |
    v

Back Cell Buffer

    |
    v

Frame Diff

    |
    v

ANSI Backend

    |
    v

Terminal
```

---

# Terminal

Responsibilities:

- Raw mode
- Alternate screen
- Cursor visibility
- Terminal size
- Input/output terminal handling
- Shutdown / restoration

Terminal should **not** know about:

- Components
- Layout
- Rendering
- Cell buffers

Example:

```cpp
auto terminal = Terminal();

if(auto result = terminal.init(); !result)
    return 1;
```

The terminal is the hardware/backend abstraction.

---

# Layout

Layout represents **geometry only**.

It does not own components.

It does not know about rendering.

It creates regions that components can occupy.

Example:

```cpp
auto root = layout::init(terminal.size());

auto [sidebar, content] =
    root.v_split(25);
```

The relationship is:

```
Region
 |
 +-- Component
 |
 +-- Component
 |
 +-- Component
```

Not:

```
Component
 |
 +-- Child Component
       |
       +-- Child Component
```

---

# Geometry Philosophy

Layout is the composition mechanism.

The UI hierarchy exists through spatial relationships, not object ownership.

Example:

```cpp
renderer.push(
    component::List(files),
    sidebar
);

renderer.push(
    component::Editor(buffer),
    content
);
```

The region defines where something exists.

The component defines what it is.

---

# Components

Components are **pure data descriptions**.

They do not render themselves.

They do not know about:

- Renderer
- CellBuffer
- Terminal
- ANSI
- Positioning

They only describe what should exist.

Example:

```cpp
auto list =
    component::List(dir_names)
    | BorderRounded;
```

Conceptually:

```cpp
struct List
{
    std::span<const std::string> items;
    ComponentFlags flags;
};
```

A component is submitted to the renderer.

It is not drawn by itself.

---

# Component Composition

Components are composed into renderer commands.

Example:

```
List Component

        |
        v

Renderer

        |
        +-- Border Command
        |
        +-- Text Commands
        |
        +-- Selection Command
        |
        v

Cell Buffer
```

Components are high-level descriptions.

Commands are low-level drawing instructions.

---

# Component Attributes

Components share common bitflags.

Example:

```cpp
enum class ComponentFlag : uint64_t
{
    None = 0,

    BorderSingle  = 1 << 0,
    BorderRounded = 1 << 1,

    Bold      = 1 << 2,
    Italic    = 1 << 3,
    Underline = 1 << 4,
};
```

Components mutate their flags through modifiers.

Example:

```cpp
auto widget =
    component::List(files)
    .BorderRounded
    .Italic;
```

No wrapper objects.

No allocations.

No inheritance.

---

# Renderer

The renderer is the Trux compositor.

Responsibilities:

- Accept components through `push()`
- Convert components into internal commands
- Resolve composition order
- Handle clipping
- Manage front/back buffers
- Track frame changes
- Produce terminal output

Example:

```cpp
renderer.begin_draw();

renderer.push(
    widget,
    region
);

renderer.end_draw();
```

---

# Renderer Pipeline

Frame lifecycle:

```
begin_draw()

        |
        v

Clear composition state

        |
        v

push(component, region)

        |
        v

Component -> Draw Commands

        |
        v

Resolve Commands

        |
        v

Write Back Buffer

        |
        v

Compare Front/Back

        |
        v

Present Changes
```

---

# Internal Draw Commands

The renderer operates on internal primitives.

Example:

```cpp
struct DrawCommand
{
    CommandType type;

    layout::Region area;

    style::Color foreground;
    style::Color background;

    ComponentFlags flags;
};
```

Possible commands:

- Text
- Fill
- Border
- Glyph
- Line
- Region copy

Components generate commands.

The renderer resolves commands into cells.

---

# Custom Components

Custom components do not require inheritance.

No:

```cpp
class MyWidget : public Component
```

Instead, users provide composition logic.

Example:

```cpp
struct CpuGraph
{
    std::vector<float> values;
    ComponentFlags flags{};
};
```

The renderer can compose it into commands.

This keeps Trux extensible without framework coupling.

---

# Rendering API

Target API:

```cpp
#include <trux/component/component.hpp>
#include <trux/layout/layout.hpp>
#include <trux/terminal/terminal.hpp>

using namespace trux;

int main()
{
    auto terminal = Terminal();
    auto renderer = renderer::Renderer(terminal.size());

    auto root = layout::init(terminal.size());

    if(auto result = terminal.init(); !result)
        return 1;

    if(auto result = terminal.enable_raw_mode(); !result)
        return 1;


    std::vector<std::string> dir_names;
    std::vector<std::string> drop_options;


    auto [split1, split2] =
        root.h_split(20);


    auto widget1 =
        component::List(dir_names)
        .BorderRounded;


    auto widget2 =
        component::Dropdown(drop_options);


    widget2.BorderSingle.Italic;


    renderer.begin_draw();

    renderer.push(widget1, split1);
    renderer.push(widget2, split2);

    renderer.end_draw();


    terminal.present(renderer);
}
```

---

# Why This Direction?

Inspired by:

- SDL's simplicity
- Ratatui's geometry system
- Raylib's frame model
- Wayland compositor buffering concepts

Borrow:

- Explicit frame boundaries
- Geometry-first composition
- Double buffering
- Command-based rendering

Avoid:

- Widget ownership trees
- Inheritance-heavy APIs
- Framework-managed application lifecycle
- Reactive object graphs

---

# Guiding Principles

1. Layout is geometry.
2. Components are descriptions.
3. Renderer is the compositor.
4. Terminal is only the backend.
5. Application owns state.
6. Push data, don't mutate hierarchies.
7. Prefer value semantics.
8. Avoid hidden ownership.
9. Prefer composition over inheritance.
10. Keep the public API simple while allowing complex internals.

---

# Current Milestones

## Completed

- Core math types
- Position / Size / Rect
- Region system
- Splitting algorithms
- Terminal abstraction
- ANSI backend
- Raw mode foundation
- PIMPL terminal implementation
- Cell buffer foundation

## Next

1. Finalize component data model
2. Implement component flags
3. Implement `Renderer::push()`
4. Design internal `DrawCommand` system
5. Build command resolver
6. Complete front/back diffing
7. Finish input/event pipeline
8. Implement standard components
9. Build Winter IDE on top

---

# Long-Term Goal

Create a modern C++23 TUI composition engine combining:

- SDL's simplicity
- Ratatui's layout model
- Raylib's frame lifecycle
- Wayland compositor-inspired buffering

while remaining lightweight, explicit, and enjoyable to build upon.
