# Keymap — chord/sequence resolution

`trux::input::Keymap<T>` sits between the raw `input::Event` stream and your
application logic. It resolves multi-key chords ("g g", "Ctrl+K Ctrl+S") into a
value of your own command type `T`, so your event-handling switch doesn't have
to track "was the last key a lone g?" by hand.

It is **not** a replacement for your normal input routing. In every example, a
component (`Menu`, `TextInput`, ...) still gets first look at each event via
`renderer.push`'s built-in dispatch. `Keymap` only ever sees events that weren't
consumed — same as the hand-written `switch(event)` blocks you'd otherwise write
for global, non-chorded keys like `q` to quit. See
[`examples/keymap_example.cpp`](examples/keymap_example.cpp) for the full
runnable version this doc is based on.

## Defining commands and binding chords

Pick an enum (or any type) for the commands your chords resolve to, and bind
sequences of keys to values of it:

```cpp
enum class Cmd { GotoTop, GotoBottom, DeleteItem, Save };

input::Keymap<Cmd> keymap;
keymap.set_timeout(std::chrono::milliseconds(600));  // this is also the default

keymap.bind({'g', 'g'}, Cmd::GotoTop);   // vim-style: gg
keymap.bind({'G'}, Cmd::GotoBottom);     // single-key chord
keymap.bind({'d', 'd'}, Cmd::DeleteItem);
```

A chord is any `std::initializer_list<char32_t>` — one key is a valid chord, and
there's no fixed limit on length. Internally each `bind()` walks/creates a trie
node per key, so `{'g','g'}` and `{'g','x'}` can coexist without either one
shadowing the other.

## Encoding modifiers

Plain ASCII keys like `'g'` and `'G'` are just their `char32_t` values. Keys
held with a modifier need to be encoded the same way
`Event::operator char32_t()` encodes a live keypress, or the bound chord will
never match:

```cpp
// code | mods << 24 -- see modifiers.hpp's operator|(char32_t, Mod)
keymap.bind({'k' | input::Mod::Ctrl, 's' | input::Mod::Ctrl}, Cmd::Save);
```

`input::Mod` is a bitflag enum (`Shift`, `Alt`, `Ctrl`, `Super`).
`operator|(char32_t, Mod)` (and the `Modifiers`-taking overload) packs the
modifier bits into the top byte of the `char32_t`, matching exactly what
`Event`'s conversion operator produces:

```cpp
constexpr operator char32_t() const noexcept {
    return code | (static_cast<char32_t>(mods.value) << 24);
}
```

So `'k' | input::Mod::Ctrl` and a live `Event` for Ctrl+K compare equal as
`char32_t`, which is what `feed()` keys its trie lookup on.

## Feeding events

Each frame, after your components have had a chance to consume the event, feed
whatever's left to the keymap:

```cpp
Cmd                resolved{};
input::ChordResult result = input::ChordResult::NoMatch;

if(event.kind == input::EventKind::Tick) {
    result = keymap.check_timeout(resolved);
} else if(!event.consumed) {
    result = keymap.feed(event, resolved);
}
```

`feed()` only looks at `EventKind::Key` presses (releases are ignored) and
returns one of three `ChordResult`s:

- **`Matched`** — the key completed a bound chord. `resolved` is set to the
  bound value and the keymap's internal state resets to the root, ready for the
  next chord.
- **`Pending`** — the key continued a chord that isn't complete yet (e.g. the
  first `g` of `gg`). Nothing is resolved yet; the keymap remembers where it is
  and waits for the next key or a timeout.
- **`NoMatch`** — the key doesn't start or continue any bound chord.

## Mismatch recovery

If a key doesn't match any child of the current pending node, `feed()` doesn't
just return `NoMatch` and drop the key. It resets to the root and **re-feeds the
same event** against the root once:

```cpp
auto it = node->children.find(key);
if(it == node->children.end()) {
    bool was_pending = (node != &m_root);
    reset();
    if(was_pending) return feed(event, out);   // retry from the root
    return ChordResult::NoMatch;
}
```

So if you've bound `{'g','g'}` and `{'x'}`, typing `g` then `x` doesn't silently
eat the `x`: the abandoned `g` prefix is dropped and `x` is evaluated fresh
against the root, matching `Cmd` for `{'x'}` as if `g` had never been pressed.
This only recurses once per call (a single re-feed against the root), so it
can't loop.

## Timeouts and `check_timeout`

A lone `g` with nothing typed after it has to eventually resolve — either as
`Cmd::GotoTop`'s prefix expiring (`NoMatch`), or, if you'd bound a value to
`{'g'}` alone, as that value matching. But nothing will call `feed()` again if
the user just... stops typing. That's what `check_timeout()` is for:

```cpp
[[nodiscard]] bool has_pending() const noexcept { return m_current != nullptr; }
[[nodiscard]] ChordResult check_timeout(T& out);
```

`check_timeout()` needs to be polled on a clock, not just on key events. The
pattern used in the example is `input::Input::set_want_tick(true)`, which makes
`poll()` return a synthetic `Event::tick()` roughly every 100ms whenever nothing
else is pending:

```cpp
input.set_want_tick(true);
// ...
if(event.kind == input::EventKind::Tick) {
    result = keymap.check_timeout(resolved);
}
```

Each `feed()` call that lands on a non-leaf node pushes the deadline out by
`set_timeout()`'s duration (600ms by default) from _that_ keypress, not from
when the chord started — so a slow-but-steady typist won't get cut off
mid-chord. `check_timeout()` only ever does something if `has_pending()` would
be true; otherwise it's a no-op `NoMatch`.

## Putting it together

A typical per-frame block, after your components' own `handle()` calls:

```cpp
Cmd                resolved{};
input::ChordResult result = input::ChordResult::NoMatch;

if(event.kind == input::EventKind::Tick) {
    result = keymap.check_timeout(resolved);
} else if(!event.consumed) {
    result = keymap.feed(event, resolved);
}

if(result == input::ChordResult::Pending) {
    status = "-- pending chord --";
} else if(result == input::ChordResult::Matched) {
    switch(resolved) {
        case Cmd::GotoTop:     /* ... */ break;
        case Cmd::GotoBottom:  /* ... */ break;
        case Cmd::DeleteItem:  /* ... */ break;
        case Cmd::Save:        /* ... */ break;
    }
} else if(event.kind == input::EventKind::Key && !event.consumed) {
    // Definitively not part of any chord (possibly after a silent
    // mismatch-recovery re-feed) -- your global, non-chorded bindings
    // live here, same as a plain switch(event) would.
    switch(event) {
        case 'q': terminal.request_quit(); break;
        default: break;
    }
}
```

## Reference

```cpp
namespace trux::input {

enum class ChordResult { NoMatch, Pending, Matched };

template <typename T> class Keymap {
public:
    void set_timeout(std::chrono::milliseconds ms) noexcept;
    [[nodiscard]] bool has_pending() const noexcept;

    void bind(std::initializer_list<char32_t> chord, T value);

    [[nodiscard]] ChordResult feed(const Event& event, T& out);
    [[nodiscard]] ChordResult check_timeout(T& out);
};

}
```

`Keymap<T>` requires nothing of `T` beyond move-assignability (it's stored in
`std::optional<T>` per trie node). Modifier encoding lives in
[`include/trux/input/modifiers.hpp`](include/trux/input/modifiers.hpp):

```cpp
enum class Mod : uint8_t { None = 0, Shift = 1<<0, Alt = 1<<1, Ctrl = 1<<2, Super = 1<<3 };

constexpr char32_t operator|(char32_t code, Mod mod) noexcept;
constexpr char32_t operator|(char32_t code, Modifiers mods) noexcept;
```
