# trux — TODO / Roadmap

Target: full-bodied backend framework for a TUI IDE (IntelliJ × nvim hybrid).
Priority order reflects what everything else routes through — fix input/focus
before building widgets on top of them.

Legend: `[ ]` not started · `[~]` partial/exists but insufficient · `[x]` done

---

## 0. Stubs to create

Nothing in the tree is stubbed yet — these are net-new files/interfaces that
should exist as skeletons (even if empty/throwing `not implemented`) so the
shape of the framework is visible before each piece is filled in.

- [x] `include/trux/input/mouse.hpp` — currently an empty file; needs a real
      `MouseEvent` struct (position, button, kind: press/release/drag/scroll,
      modifiers) and a parse entry point mirrored off `EventParser`.
- [~] `include/trux/input/modifiers.hpp` — `Modifiers` bitflag (Ctrl/Alt/Shift/Super)
      to attach to `Event`, currently nonexistent — `Event` only carries `code`.
- [ ] `include/trux/input/keymap.hpp` — chord/sequence resolver interface
      (`feed(Event) -> optional<Command>`), sits between `EventParser` and
      component `handle()`.
- [ ] `include/trux/command/command.hpp` — `Command` type (id + payload) that
      keymaps resolve to and components/app logic consume instead of raw `Event`.
- [ ] `include/trux/focus/focus_manager.hpp` — owns the focused component
      path/id, exposes `focus(id)`, `focused() const`, tab-order traversal.
- [ ] `include/trux/component/text_area.hpp` — the actual editor buffer
      component; currently no editable multi-line widget exists at all.
- [ ] `include/trux/component/floating.hpp` — popup/overlay wrapper
      (z-ordered region outside the split tree) for autocomplete, hover,
      command palette.
- [ ] `include/trux/style/span.hpp` — `StyledSpan{ start, end, style, fg, bg }`
      so a single `DrawText` can carry mixed styling for syntax highlighting.
- [ ] `include/trux/style/theme.hpp` — palette/theme abstraction; currently
      colors are hardcoded per-`Cell`/`DrawText` construction site.
- [ ] `include/trux/terminal/capabilities.hpp` — truecolor / kitty-keyboard /
      bracketed-paste detection results, queried once at `Terminal::init()`.
- [ ] `include/trux/loop/event_loop.hpp` — multi-fd poll/epoll loop
      abstraction; `Input::poll(Terminal&)` currently only reads stdin
      synchronously in a blocking loop.
- [ ] `include/trux/clipboard/clipboard.hpp` — OSC 52 read/write wrapper.
- [ ] `tests/` — no test directory exists yet; needs headless `CellBuffer`
      snapshot testing before the diff/resolve pipeline gets more complex.

---

## 1. Focus & input routing (blocking everything else)

- [x] Add `FocusManager` owning current focused component id/path.
- [~] `Renderer::push` should register handlers under a scope/id, not just
      append to a flat `m_handlers` vector.
- [ ] `Renderer::dispatch` should route to the focused scope first, with an
      optional secondary "global" tier for app-wide bindings (Ctrl+P, etc.)
      instead of last-registered-first-to-return-true across everyone.
- [ ] Tab/Shift-Tab (or explicit `focus_next()`/`focus_prev()`) traversal
      order across visible components.
- [ ] Decide whether `Container` needs focus-awareness (e.g. clicking a pane
      should focus it) — ties into mouse work below.
- [ ] Un-swallow-consumed-events bug (`Input::next_unconsumed`) — fixed in
      this session, keep the regression covered by a test once `tests/` exists.

## 2. Keymap / chord resolution

- [ ] `Keymap` type: accumulates `Event`s into sequences (`dd`, `gg`),
      handles numeric counts (`3j`), and resolves to `Command`.
- [ ] Mode concept (Normal/Insert/Visual/Command-line at minimum) that
      changes which keymap is active — modal editing needs this before
      `TextArea` bindings make sense.
- [ ] Timeout handling for ambiguous prefixes (e.g. `g` waiting to see if
      `gg` follows) — needs a clock/timer, ties into the event loop work.
- [ ] Rebindable/user-configurable keymap loading (even if just a C++ builder
      API to start, no config file format required yet).
- [ ] Migrate `Menu::handle` and future widgets off raw `switch(event.code)`
      once `Command` exists, so components consume resolved commands, not
      bytes.

## 3. Terminal input protocol coverage

- [x] Modifier keys — CSI-u / Kitty keyboard protocol parsing so Ctrl/Alt/Shift
      combos are distinguishable (currently only 4 bare arrow keys via
      `CSI A-D`, no modifiers at all).
- [x] Function keys, Home/End/PageUp/PageDown/Delete/Insert.
- [ ] Bracketed paste mode (`CSI 200~` / `201~`) — without this, pasting text
      gets character-by-character fed through the keymap resolver.
- [ ] Query + fall back gracefully when the terminal doesn't support the
      Kitty protocol (most don't) — needs `capabilities.hpp` above.
- [ ] Focus-in/focus-out terminal events (`CSI I` / `CSI O`), useful for
      pausing cursor blink / autosave-on-blur later.

## 4. Async event loop

- [ ] Replace blocking `::read(STDIN_FILENO, ...)` single-fd loop with
      `poll`/`epoll` over multiple fds.
- [ ] Support for registering additional fds (LSP server pipe/socket, file
      watcher fd) alongside stdin.
- [ ] Timer support (debounced diagnostics, chord-prefix timeout above,
      cursor blink).
- [ ] Decide sync-callback vs. some lightweight task/future model for
      LSP responses arriving async mid-frame.

## 5. SIGWINCH / resize

- [ ] Register `SIGWINCH` handler alongside existing `SIGINT`/`SIGTERM`.
- [ ] Wire resize signal through to `Renderer::resize()` + force full
      redraw (`front`/`back` buffers need re-sizing and re-diffing from
      scratch after a resize).
- [ ] Re-query `Terminal::size()` on resize rather than only at startup.

## 6. Text editing primitive (`TextArea`)

- [ ] Buffer representation — pick gap buffer / rope / piece table (matters
      for large-file performance, worth deciding before writing code).
- [ ] Cursor as line/col, not flat index; multi-cursor is a stretch goal,
      don't block on it.
- [ ] Insert/delete/undo-redo stack.
- [ ] Selection ranges (char-wise, line-wise, block-wise for later Vim
      visual-block parity).
- [ ] Vertical scroll (existing `scroll_offset` pattern from `Menu`/`List`
      is reusable) **and** horizontal scroll for long lines — nothing in
      the framework currently scrolls on the x-axis.
- [ ] Line wrapping (optional/toggleable) vs. fixed horizontal scroll.
- [ ] Line-number gutter rendering.
- [ ] Render via `StyledSpan`s instead of one `DrawText` per full line, so
      syntax highlighting doesn't require manual token-splitting into many
      `DrawText` pushes at the call site.

## 7. Styling / theming

- [ ] `StyledSpan` type + `DrawCommand` variant (or extend `DrawText`) to
      carry mixed styling within one string.
- [ ] `Theme`/palette abstraction — named colors (`editor.bg`, `syntax.keyword`,
      etc.) resolved through a theme instead of literal `Color{r,g,b}` at
      each call site.
- [ ] 256-color / no-color fallback path for terminals without truecolor —
      `sgr_codes` in `terminal.cpp` currently always emits `38;2;r;g;b` truecolor
      codes unconditionally.

## 8. Layered / floating regions

- [ ] `Floating` component or renderer-level overlay stack, drawn after the
      main split tree, for autocomplete popups, hover/diagnostic boxes,
      command palette.
- [ ] Z-order + dismiss-on-focus-loss / dismiss-on-outside-click semantics.
- [ ] Damage/diff still needs to work correctly when overlays cover part of
      an already-drawn region (currently `resolve()` just draws in command
      order, which should already handle this, but needs explicit testing
      once floats exist).

## 9. Mouse

- [ ] SGR mouse mode parsing (`CSI ?1006h` enable, `CSI < b;x;yM/m` events).
- [ ] Click-to-position (route through focus manager — clicking a pane
      focuses it).
- [ ] Drag-select (ties into `TextArea` selection).
- [ ] Scroll wheel → scroll_offset adjustment on the component under the
      cursor.

## 10. Wide/CJK glyph handling

- [ ] `wcwidth`-equivalent lookup so double-width glyphs (CJK, some emoji)
      correctly occupy two `Cell` columns instead of one — `resolve()`'s
      `DrawText` loop currently advances `pos.x` by exactly one per decoded
      codepoint regardless of display width.
- [ ] Combining character handling (zero-width) — lower priority than the
      double-width case.

## 11. Clipboard

- [ ] OSC 52 write (copy from buffer → host clipboard).
- [ ] OSC 52 read where terminal supports it (most require explicit
      opt-in from the user's terminal config — document this limitation).

## 12. Terminal capability negotiation

- [ ] Truecolor support detection (`COLORTERM` env var + optional query).
- [ ] Kitty keyboard protocol support query (`CSI ?u` / `CSI ?<flags>u`).
- [ ] Bracketed paste support — generally safe to assume, but gate it
      behind a capability flag rather than hardcoding.

## 13. Testing

- [ ] Headless `CellBuffer` snapshot tests (build a frame, diff, assert
      expected `CellDiff`s) — would have caught the swallowed-event bug
      from this session at the `Input`/dispatch layer instead of by hand
      with `fprintf` tracing.
- [ ] `EventParser` unit tests per byte sequence (arrows, escape, CSI,
      once modifiers/function-keys land).
- [ ] Component `handle()` tests independent of rendering (selection
      wrap-around, scroll-follow behavior — the exact class of off-by-one
      bug already found in `Menu::handle`'s wraparound bounds).
