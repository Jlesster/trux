# trux — TODO / Roadmap

Target: a general-purpose terminal composition and rendering engine — terminal
handling, layout, focus/input routing, and rendering primitives that any
terminal application can be built on top of. Priority order reflects what
everything else routes through — fix input/focus before building more on top of
them.

Legend: `[ ]` not started · `[~]` partial/exists but insufficient · `[x]` done

---

## Status check

The core pipeline is more complete than this file used to reflect, in both
directions — some things marked done weren't fully wired, and several things
marked unstarted (resize/SIGWINCH, wide-glyph width, parts of the async story)
already work. Two things currently block calling this stable:

1. **Two failing tests, and both hide most of their own test binary.** Neither
   `test::run` nor the individual tests catch anything — a failed `assert()`
   calls `abort()` mid-process, so every test scheduled _after_ the failing one
   in that binary's `main()` never runs at all. Concretely: `widget_test` fails
   on its 1st test, so its other 12 never execute; `event_parser_test` fails on
   its 3rd, so its other 16 (including all the mouse and bracketed-paste tests)
   never execute either. "7/9 test binaries pass" is true but overstates how
   much is actually test-verified right now — most of the suite doesn't run.
2. **Section 0's stub list is still mostly empty on disk.** No `Keymap`, no
   `Command` type, no `Floating`, no `StyledSpan`/`Theme`, no capability
   negotiation, no clipboard, and `Input::poll`'s multi-fd support is half-wired
   (see §4).

## Known test failures (fix before relying on this suite)

Confirmed on a clean build (GCC 14, `-std=gnu++23`): 7/9 test binaries pass, but
see the note above about what that number doesn't tell you.

- [ ] **`widget_test` — `label_test` aborts, and everything after it in that
      binary never runs.** `assert(back_buffer().at({0,     0}).glyph == U'H')`
      fails. `component::Label::build()` writes the first glyph at
      `area.position().x + 1`, one cell right of the component's own origin.
      Checked every other component for the same pattern — nothing else has it.
      `TextInput`'s similar-looking `pos.x     + 1` is intentional (it's
      reserving column 0 for a cursor block), so this really does look isolated
      to `Label`.
- [ ] **`event_parser_test` — `test_standalone_escape_and_reprocess` aborts, and
      16 later tests in that binary never run (including all mouse and
      bracketed-paste tests).** This one isn't a straightforward bug — it's two
      incompatible models colliding. `State::Escape` currently treats `ESC`
      immediately followed by a printable byte (32–126) as `Alt+<that key>`,
      which is how a lot of terminals encode Alt combinations, and is presumably
      deliberate — `resolve_pending()` already handles the _true_ "bare ESC,
      nothing followed" case correctly via a timeout path. The test instead
      asserts that `ESC` immediately followed by _any_ non-`[` byte — including
      an ordinary printable one — should resolve to a standalone `Key::Escape`
      and queue that byte for reprocessing. Only one of these can be the
      intended behavior for a printable byte after `ESC`; worth deciding which,
      since fixing this by making the test pass would silently remove Alt-key
      decoding.
- [ ] **Test harness swallows output and hides everything past the first
      failure.** `test::run` (`tests/test.hpp`) just calls `fn()` directly — no
      try/catch, no per-test isolation. Worth having it catch exceptions and/or
      moving to per-test processes (or at least a signal handler that reports
      which test aborted and continues) before trusting aggregate pass counts on
      a growing suite.

Building requires **GCC 14+** — confirmed by testing directly; GCC 13's
libstdc++ is missing `<print>` and `<expected>`, so `ansi.cpp` and
`terminal.cpp` fail to compile on it (e.g. stock Ubuntu 24.04's default
compiler). Note that `CMakeUserPresets.json` in this repo actually points at
`clang++` — that toolchain wasn't available to verify here, so the Clang version
actually needed to build this is currently unconfirmed; worth pinning down and
stating explicitly once known.

---

## 0. Stubs to create

- [x] `include/trux/input/mouse.hpp` — `MouseEvent` (position, button, kind,
      modifiers) exists and is parsed (see §9). Note: the tests that would
      confirm the parsing currently never run — see above.
- [x] `include/trux/input/modifiers.hpp` — `Modifiers` bitflag (Ctrl/Alt/
      Shift/Super) exists, attached to `Event`.
- [ ] `include/trux/input/keymap.hpp` — chord/sequence resolver interface
      (`feed(Event) -> Result`), sits between `EventParser` and component
      `handle()`.
- [ ] `include/trux/command/command.hpp` — a `Command` type (id + payload) that
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
      just never asks what's actually supported.)
- [~] `include/trux/loop/event_loop.hpp` — doesn't exist as its own file, but
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

## 2. Keymap / chord resolution

- [ ] `Keymap` type: accumulates `Event`s into sequences, handles numeric
      counts, resolves to a `Command`.
- [ ] Mode concept (at least two modes with different active bindings) — needed
      before multi-key chorded bindings are worth much.
- [ ] Timeout handling for ambiguous prefixes (a key that could be the start of
      a longer chord, waiting to see what follows) — needs a clock/timer, ties
      into the event loop work below (currently zero `<chrono>`/`timerfd` usage
      anywhere in the codebase).
- [ ] Rebindable/user-configurable keymap loading (a C++ builder API is fine to
      start; no config file format required yet).
- [ ] Migrate `Menu::handle` and future widgets off raw `switch(event.code)`
      once `Command` exists.

## 3. Terminal input protocol coverage

- [x] Modifier keys — CSI-u / Kitty keyboard protocol parsing.
- [x] Function keys, Home/End/PageUp/PageDown/Delete/Insert.
- [x] Bracketed paste mode (`CSI 200~` / `201~`) — parsed in
      `EventParser::State::Paste`, resolves to `Event::from_paste`. (Its tests
      exist but currently don't run — see "Known test failures.")
- [ ] Query + fall back gracefully when the terminal doesn't support the Kitty
      protocol (most don't) — needs `capabilities.hpp` above.
- [ ] Focus-in/focus-out terminal events (`CSI I` / `CSI O`) — no trace of these
      anywhere in the parser.

## 4. Async / event loop

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

## 5. Resize handling

Already fully wired, contrary to what this file previously said:

- [x] `SIGWINCH` handler installed (alongside `SIGINT`/`SIGTERM`), sets an
      async-signal-safe atomic flag.
- [x] `Input::poll` checks `terminal.resized()` each loop iteration and pushes a
      `Resize` event carrying a freshly re-queried `terminal.size()` — not just
      the size from startup.
- [x] `Renderer::dispatch` routes `Resize` events to `Renderer::resize()`, which
      resizes both cell buffers and calls `m_front.invalidate()`, forcing every
      cell to re-diff as changed on the next frame (i.e. a full redraw). No
      dedicated test for this path yet, but by reading the code end-to-end this
      looks correctly connected.

## 6. Styling / theming

- [ ] `StyledSpan` type + `DrawCommand` variant (or extend `DrawText`) for mixed
      styling within one string.
- [ ] `Theme`/palette abstraction — named colors resolved through a theme
      instead of literal `Color{r,g,b}` at each call site.
- [ ] 256-color / no-color fallback path — `util::sgr_codes` always emits
      `38;2;r;g;b` truecolor codes unconditionally, confirmed by reading it
      directly; no conditional path exists.

## 7. Layered / floating regions

- [ ] `Floating` component or renderer-level overlay stack, drawn after the main
      split tree.
- [ ] Z-order + dismiss-on-focus-loss / dismiss-on-outside-click semantics.
- [ ] Damage/diff correctness when overlays cover part of an already-drawn
      region — needs an explicit test once floats exist.

## 8. Mouse

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

## 9. Wide/CJK glyph handling

Already done, contrary to what this file previously said:

- [x] `util::glyph_width()` is a real wcwidth-equivalent: returns 0 for
      combining-mark ranges, 2 for CJK/Hangul/emoji ranges, 1 otherwise. It's
      actually used — in the `resolve()` `DrawText` loop, in the front/back
      diff's `running_x` accounting, and in `TextInput`'s column-width math. Not
      a stub, not unused.
- [ ] Nothing currently exercises it in a test — worth a `glyph_width` unit test
      and a render test with an actual wide glyph, given how easy this class of
      thing is to silently regress.

## 10. Clipboard

- [ ] OSC 52 write (copy from buffer → host clipboard).
- [ ] OSC 52 read where terminal supports it (document the opt-in limitation
      most terminals impose).

## 11. Terminal capability negotiation

- [ ] Truecolor support detection (`COLORTERM` env var + optional query).
- [ ] Kitty keyboard protocol support query (`CSI ?u` / `CSI ?<flags>u`).
- [ ] Bracketed paste / mouse-mode support detection — currently enabled
      unconditionally in `ansi.cpp` (`?2004h`, `?1002h`, `?1006h`) rather than
      gated behind a capability flag.

## 12. Testing

- [x] `tests/` directory with CTest wiring exists — 9 binaries.
- [ ] Fix the abort-and-skip problem in the test harness itself (see "Known test
      failures") before trusting pass/fail counts on a larger suite.
- [ ] Fix the 2 currently-failing assertions.
- [ ] Add coverage for `Checkbox`, `Dialog`, `Paragraph`, `Split`, and
      `TextInput` — all five exist as real components (not stubs) but none
      currently have any tests.
- [ ] Add a test for `async::Channel<T>` and for the `Input::poll` `extra_fds`
      wiring once §4 is fixed.
- [ ] Add a resize-path test (SIGWINCH → `Resize` event → buffer invalidation →
      full redraw) — currently only verified by reading the code, not by a test.
