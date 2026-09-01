# trux — TODO / Roadmap

Target: a general-purpose terminal composition and rendering engine — terminal
handling, layout, focus/input routing, and rendering primitives that any
terminal application can be built on top of. Priority order reflects what
everything else routes through — fix input/focus before building more on top of
them.

Legend: `[ ]` not started · `[~]` partial/exists but insufficient · `[x]` done

---

## 0. Stubs to create

- [x] `include/trux/input/mouse.hpp` — `MouseEvent` (position, button, kind,
      modifiers) exists and is parsed (see §9). Note: the tests that would
      confirm the parsing currently never run — see above.
- [x] `include/trux/input/modifiers.hpp` — `Modifiers` bitflag (Ctrl/Alt/
      Shift/Super) exists, attached to `Event`.
- [x] `include/trux/input/keymap.hpp` — chord/sequence resolver interface
      (`feed(Event) -> Result`), sits between `EventParser` and component
      `handle()`.
- [x] `include/trux/command/command.hpp` — a `Command` type (id + payload) that
      keymaps resolve to and app logic consumes instead of raw `Event`.
- [x] `include/trux/focus/focus_manager.hpp` — owns focused id, tab-order
      traversal (`focus_next`/`focus_prev`), wired into `Renderer::dispatch`.
- [ ] `include/trux/component/floating.hpp` — popup/overlay wrapper (z-ordered
      region outside the split tree).
- [ ] `include/trux/style/span.hpp` —
      `StyledSpan{ start, end, style, fg,     bg }` so a single `DrawText` can
      carry mixed styling within one string.
- [ ] `include/trux/style/theme.hpp` — palette/theme abstraction; colors are
      still hardcoded per-`Cell`/`DrawText` construction site.
- [ ] `include/trux/terminal/capabilities.hpp` — truecolor / kitty-keyboard /
      bracketed-paste detection, queried once at `Terminal::init()`. (Terminal
      already _emits_ the enabling sequences for several of these — see §12 — it
      just never asks what's actually supported.) [~]
      `include/trux/loop/event_loop.hpp` — doesn't exist as its own file, but
      the pieces are mostly there and just not fully connected: see §4.
- [ ] `include/trux/clipboard/clipboard.hpp` — OSC 52 read/write wrapper.
- [x] `tests/` — exists, 9 test binaries across cell buffer, splitting,
      rendering, region, input, focus, widgets, event parsing, and async. 2
      currently abort partway through — see above.

---

## 1. Focus & input routing

- [x] `FocusManager` owning current focused component id/path.
- [x] `Renderer::push` registers handlers under a scope/id
      (`Handler{id,     region, modal, fn}`), not a flat unscoped vector.
- [ ] `Renderer::dispatch` has a `modal` tier and a `focused` tier, but no third
      "global" tier for app-wide bindings that should fire regardless of what's
      focused — right now an unfocused/no-match event just returns `false`.
- [x] Tab/Shift-Tab traversal (`focus_next()`/`focus_prev()`) across visible
      components — implemented and wired into `dispatch`.
- [x] Mouse click focuses the component under the cursor (`dispatch`'s
      `MouseKind::Press` branch calls `m_focus.focus(it->id)`).
- [ ] The previously-mentioned swallowed-event regression in
      `Input::next_unconsumed` has no dedicated test — `input_test.cpp` only
      covers basic `Event`/queue construction, not dispatch/consume behavior.
      Don't assume it's still fixed without one.

## 2. Terminal input protocol coverage

- [x] Modifier keys — CSI-u / Kitty keyboard protocol parsing.
- [x] Function keys, Home/End/PageUp/PageDown/Delete/Insert.
- [x] Bracketed paste mode (`CSI 200~` / `201~`) — parsed in
      `EventParser::State::Paste`, resolves to `Event::from_paste`. (Its tests
      exist but currently don't run — see "Known test failures.")
- [ ] Query + fall back gracefully when the terminal doesn't support the Kitty
      protocol (most don't) — needs `capabilities.hpp` above.
- [ ] Focus-in/focus-out terminal events (`CSI I` / `CSI O`) — no trace of these
      anywhere in the parser.

## 3. Async / event loop

More already works here than the previous version of this file gave credit for,
but there's one real, specific gap:

- [x] `Terminal::wait_readable(primary_fd, extra_fds, timeout_ms)` — a real
      `::poll()` over an arbitrary set of fds, correctly reports which one was
      ready via `last_ready_fd()`.
- [x] `async::Executor` — spawns work on a background `std::jthread`, delivers
      completion callbacks back on the main thread via an `eventfd`, drained by
      `run_pending()`. 4 passing tests.
- [x] `async::Channel<T>` — generic `eventfd`-backed producer/consumer queue,
      independent of `Executor`. Exists, but has no tests of its own, and:
- [ ] **`Input::poll(Terminal&, extra_fds)` silently ignores the `extra_fds`
      parameter it takes.** The body only ever builds `wait_readable`'s fd set
      from `async::Executor::instance().fd()` — an app-registered fd (e.g. a
      `Channel` backing some other async source) passed into `poll()` is never
      actually included in the `poll()` call, so it can't wake the loop even
      though `wait_readable` itself fully supports it. This is a one-line-ish
      wiring fix, not a missing subsystem — but worth a test once fixed, since
      nothing currently would catch a regression here.
- [ ] Timer support (debounced work, chord-prefix timeout above) — no `timerfd`
      or `<chrono>` deadline anywhere yet.

## 4. Styling / theming

- [ ] `StyledSpan` type + `DrawCommand` variant (or extend `DrawText`) for mixed
      styling within one string.
- [ ] `Theme`/palette abstraction — named colors resolved through a theme
      instead of literal `Color{r,g,b}` at each call site.
- [ ] 256-color / no-color fallback path — `util::sgr_codes` always emits
      `38;2;r;g;b` truecolor codes unconditionally, confirmed by reading it
      directly; no conditional path exists.

## 5. Layered / floating regions

- [ ] `Floating` component or renderer-level overlay stack, drawn after the main
      split tree.
- [ ] Z-order + dismiss-on-focus-loss / dismiss-on-outside-click semantics.
- [ ] Damage/diff correctness when overlays cover part of an already-drawn
      region — needs an explicit test once floats exist.

## 6. Mouse

- [x] SGR mouse mode parsing (`CSI ?1006h` enable, `CSI < b;x;yM/m` events) —
      `finish_sgr_mouse` in `event_parser.cpp` decodes button, kind
      (press/release/drag/scroll), and position. (Tests exist but currently
      don't run — see "Known test failures.")
- [x] Click-to-focus — routed through `FocusManager` in `dispatch`.
- [ ] Drag-select — no selection concept exists anywhere yet to drag over.
- [ ] Scroll wheel → `scroll_offset` adjustment on the component under the
      cursor — events parse correctly (confirmed by reading `finish_sgr_mouse`)
      but nothing anywhere consumes `MouseKind::ScrollUp`/`ScrollDown` to
      actually move a `scroll_offset`.

## 7. Clipboard

- [ ] OSC 52 write (copy from buffer → host clipboard).
- [ ] OSC 52 read where terminal supports it (document the opt-in limitation
      most terminals impose).

## 8. Terminal capability negotiation

- [ ] Truecolor support detection (`COLORTERM` env var + optional query).
- [ ] Kitty keyboard protocol support query (`CSI ?u` / `CSI ?<flags>u`).
- [ ] Bracketed paste / mouse-mode support detection — currently enabled
      unconditionally in `ansi.cpp` (`?2004h`, `?1002h`, `?1006h`) rather than
      gated behind a capability flag.

## 9. Testing

- [x] `tests/` directory with CTest wiring exists — 9 binaries.
- [ ] Add coverage for `Checkbox`, `Dialog`, `Paragraph`, `Split`, and
      `TextInput` — all five exist as real components (not stubs) but none
      currently have any tests.
- [ ] Add a test for `async::Channel<T>` and for the `Input::poll` `extra_fds`
      wiring once §4 is fixed.
- [ ] Add a resize-path test (SIGWINCH → `Resize` event → buffer invalidation →
      full redraw) — currently only verified by reading the code, not by a test.
