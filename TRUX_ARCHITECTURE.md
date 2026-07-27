# Trux Architecture Notes

## Vision

Trux is **not** a widget framework like FTXUI. It is a lightweight terminal
composition and rendering engine in the same spirit that SDL is a rendering
library.

Winter (the IDE) will be built **on top of** Trux.

Core philosophy:

- Terminal manages the terminal.
- Layout manages geometry.
- Renderer manages drawing.
- Components know how to draw themselves.
- The application owns all components and state.

---

# Design Goals

- Ergonomic C++23 API.
- Immediate-mode feeling.
- Internally double buffered.
- Geometry-first composition.
- No React-style component tree.
- No hidden ownership.
- No framework magic.

---

# High-Level Architecture

Application ↓ Layout (geometry) ↓ Renderer ↓ Back Cell Buffer ↓ Diff Renderer ↓
ANSI Backend ↓ Terminal

---

# Terminal

Responsibilities:

- Raw mode
- Alternate screen
- Cursor visibility
- Terminal size
- Shutdown / restoration

Terminal should **not** own rendering.

---

# Renderer

Responsibilities:

- begin_draw()
- end_draw()
- Own front/back cell buffers
- Diff frames
- Emit ANSI
- Clip drawing to a region

The renderer is effectively the compositor.

---

# Components

Components are renderable objects.

They do **not** own children.

They do **not** own layout.

Example direction:

```cpp
class Component {
public:
    virtual ~Component() = default;
    virtual void render(Renderer&, Region area) = 0;
};
```

Application owns components.

Renderer receives components.

---

# Layout Philosophy

The biggest discovery was that layout should represent **geometry**, not
ownership.

Avoid:

Component └── Component └── Component

Prefer:

Region ↓ Split ↓ Regions

---

# Geometry DSL

Preferred style:

```cpp
auto root = layout::root(terminal.size());

auto [sidebar, content] =
    root.v_split(25);

auto editor =
    content.h_split(80);

renderer.push(file_tree, sidebar);
renderer.push(editor_view, editor[0]);
renderer.push(console_view, editor[1]);
```

Or chained:

```cpp
auto panes =
    layout::root(terminal.size())
        .margin(1)
        .v_split(30);
```

---

# Region Operations

Initial API ideas:

- v_split(percent)
- h_split(percent)
- margin(...)
- centered(width, height)
- left(cols)
- right(cols)
- top(rows)
- bottom(rows)

All operations return new Region objects.

No mutation.

---

# Why This Direction?

Inspired by Ratatui's layout engine, but **not** its framework design.

Steal:

- Recursive space partitioning
- Elegant slicing API

Do not copy:

- Widget ownership tree
- Framework-controlled hierarchy

---

# Rendering Model

Frame lifecycle:

begin_draw()

↓

Application submits components

↓

Components render into back buffer

↓

end_draw()

↓

Diff against front buffer

↓

Emit ANSI

This provides immediate-mode ergonomics with internally buffered rendering.

---

# Guiding Principles

1.  Layout is geometry.
2.  Components are dumb renderables.
3.  Renderer is the compositor.
4.  Terminal only manages the terminal.
5.  Application owns everything.
6.  Favor value semantics.
7.  Avoid hidden state.
8.  Prefer expressive geometry over coordinate arithmetic.

---

# Current Milestones

Completed

- Core math types
- Terminal
- ANSI backend
- Raw mode
- PIMPL
- Smart pointers

Next

1.  Layout module
2.  Region implementation
3.  Split algorithms
4.  Renderer interface
5.  Cell buffer
6.  Diff renderer
7.  Input/events
8.  Widgets
9.  Winter IDE

---

# Long-Term Goal

Create a modern, ergonomic C++23 TUI engine that combines:

- SDL's simplicity
- Ratatui's geometry system
- Raylib's frame lifecycle
- Wayland compositor-inspired buffering

while remaining lightweight and enjoyable to build upon.
