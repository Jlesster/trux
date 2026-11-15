/// @file dialog.hpp
/// @brief Dialog: a modal overlay with a centered, optionally-titled
///        and bordered box containing either an embedded TextInput
///        (input dialog) or static word-wrapped text (popup); typically
///        pushed with `modal = true` (see Renderer::push()).

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component.hpp"
#include "trux/component/look.hpp"
#include "trux/component/paragraph.hpp"
#include "trux/component/text_input.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace trux::component {

/// The `size`-sized Region centered within `root`, clamped so it
/// never exceeds `root`'s bounds.
[[nodiscard]]
inline layout::Region centered(layout::Region root, layout::Size size) {
    auto [rw, rh] = root.size();
    auto pos = root.position();
    int w = std::min(size.width, rw);
    int h = std::min(size.height, rh);
    return layout::Region{{pos.x + (rw - w) / 2, pos.y + (rh - h) / 2}, {w, h}};
}

struct Dialog;

/// Every settable member of Dialog, as a plain aggregate so a dialog
/// can be configured in one braced designated-init expression (a
/// class with constructors can't be; see Dialog::operator=). Fields
/// not named reset to their defaults, so name `mode` too. Field
/// order matters for designated init: mode, size, title, flags,
/// border_color*, look, dismissable, on_key, on_confirm, body, input,
/// open.
struct DialogFields {
    /// Which content the dialog shows; set by Dialog's constructors.
    enum class Mode { Input, Text };

    /// Text shows `body`, Input shows `input`.
    Mode mode = Mode::Input;
    /// Size of the dialog box, regardless of the root area it's built into.
    layout::Size size{40, 7};
    /// Optional title; omitted if empty. Drawn on the border edge when
    /// the dialog has a border, otherwise as a bold line above the content.
    Title title;
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};
    /// Border color used when unfocused (see component::BorderColor).
    std::optional<style::Color> border_color_fixed;
    /// Border color used while focused; falls back to
    /// `border_color_fixed` when unset.
    std::optional<style::Color> border_color_active;
    /// The Look this dialog was styled with. Stored so it survives a
    /// DialogFields brace (which resets unnamed members), and applied by
    /// Dialog's constructors and by `Dialog(DialogFields)` /
    /// `operator=(DialogFields)` (see Dialog::apply()). Set it in a brace
    /// to style the dialog there: `.look = base.titled({"Title"})`.
    Look look;
    /// Text mode: if false, Escape/Enter do nothing and the owner
    /// closes the dialog (`open = false`), e.g. from `on_key` or a
    /// completion event.
    bool dismissable = true;
    /// Text mode: sees every key press before the built-in
    /// Escape/Enter. Return true to consume it. Receives the dialog so
    /// it can close it without capturing it.
    std::function<bool(Dialog&, const input::Event&)> on_key{};
    /// Called when Enter is pressed, before the dialog closes.
    std::function<void()> on_confirm{};
    /// Text mode content. Plain Paragraph, so `body.text` is assignable.
    Paragraph body;
    /// Input mode content. Its `flags` and `border_color` are
    /// independent of the dialog's own.
    TextInput input;
    /// Whether the dialog is showing. Cleared by Escape and Enter;
    /// callers set it to show the dialog and skip pushing it when false.
    bool open = false;
};

/// A modal box that fills a fixed `size` centered within whatever
/// area it's built into, drawing an opaque background, an optional
/// border, an optional title, and its content below it. The content
/// is chosen by `mode` (set by the constructors, or by `.mode` in a
/// DialogFields brace):
///
///  - Input: an embedded TextInput. Escape closes the dialog; Enter
///    runs `on_confirm` and then closes it; every other event goes to
///    `input`.
///  - Text: static `body` text (word-wrapped, Up/Down scroll), for
///    notifications and status indicators. Key presses go to `on_key`
///    first; unless `dismissable` is false, Escape then closes the
///    dialog and Enter runs `on_confirm` and closes it.
///
/// Everything is owned: `open` is the dialog's own visibility flag,
/// and the content lives in `input` / `body` (standalone primitives;
/// Dialog just holds them), so `dialog.input.value` and
/// `dialog.body.text` are readable and writable directly, e.g. from
/// `on_confirm`. The members live in DialogFields.
///
/// Styling comes from the `look` member (a Look: border, colours,
/// title). The Look constructors store it and apply it, and it is
/// re-applied by `Dialog(DialogFields)` and `operator=(DialogFields)`,
/// so a look named in a DialogFields brace is not lost when the rest of
/// the fields reset. A brace with no `look` leaves the look empty and
/// the dialog at its default styling.
struct Dialog : DialogFields {
    /// Input dialog (a TextInput in a centered box).
    Dialog() = default;
    /// Input dialog of the given size.
    explicit Dialog(layout::Size box) { size = box; }
    /// Same as above, additionally applying `look` (border, colours, title).
    Dialog(layout::Size box, const Look& look)
        : Dialog(box) {
        this->look = look;
        apply();
    }
    /// Input dialog styled with `look` (border, colours, title). The look
    /// is stored in `.look`, so it survives a later DialogFields brace.
    Dialog(const Look& look) {
        this->look = look;
        apply();
    }

    /// Static popup showing `text`, word-wrapped and Up/Down scrollable.
    /// `box` has no default: `Dialog({w, h})` would become ambiguous.
    Dialog(std::string text, layout::Size box) {
        mode = Mode::Text;
        size = box;
        body.text = std::move(text);
    }
    /// Same as above, additionally applying `look` (border, colours, title).
    Dialog(std::string text, layout::Size box, const Look& look)
        : Dialog(std::move(text), box) {
        this->look = look;
        apply();
    }

    /// Every member from a braced designated-init list:
    /// `Dialog d({.mode = Dialog::Mode::Text, .dismissable = false})`.
    /// A `look` named in the list is applied here.
    explicit Dialog(DialogFields fields)
        : DialogFields(std::move(fields)) {
        apply();
    }

    /// Assigns every member from a braced designated-init list:
    /// `dialog = {.mode = Dialog::Mode::Text, .dismissable = false}`.
    /// Like aggregate assignment, members not named reset to their
    /// defaults. (Dialog itself isn't an aggregate, so the list goes
    /// to DialogFields.) A `look` named in the list is applied after the
    /// copy, so the dialog keeps its styling across a re-assignment.
    Dialog& operator=(DialogFields fields) {
        static_cast<DialogFields&>(*this) = std::move(fields);
        apply();
        return *this;
    }

    /// Centers the dialog within `root`, fills it opaquely, draws the
    /// optional border and title, and builds `input` (Input mode) or
    /// `body` (Text mode) into the remaining space.
    void build(layout::Region root, renderer::DrawCommandBuffer& cmd) const {
        auto area = centered(root, size);
        cmd.push(renderer::Fill{area, renderer::Cell{}});

        auto inner = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            build_title(area, title, border_color, cmd);
            inner = inset(area);
        } else if(!title.text.empty()) {
            cmd.push(renderer::DrawText{
                inner.position(), title.text, style::Style::Bold});

            inner =
                layout::Region{{inner.position().x, inner.position().y + 1},
                               {inner.size().width, inner.size().height - 1}};
        }

        if(mode == Mode::Input) input.build(inner, cmd);
        else body.build(inner, cmd);
    }

    /// Input mode: Escape closes the dialog and is always consumed.
    /// Enter runs `on_confirm`, closes the dialog, and is always
    /// consumed. All other events are forwarded to `input.handle()`.
    ///
    /// Text mode: key presses (not releases) go to `on_key`; if it
    /// returns true they are consumed. Otherwise, when `dismissable`,
    /// Escape closes the dialog and Enter runs `on_confirm` and closes
    /// it; remaining keys scroll `body`. All key events are consumed
    /// (the dialog is modal); non-key events are not.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(mode == Mode::Text) {
            if(event.kind != input::EventKind::Key) return false;
            const bool live = event.key_state != input::KeyState::Release;

            if(live && on_key && on_key(*this, event)) return true;
            if(live && dismissable) {
                if(event.code == input::Key::Escape) {
                    open = false;
                    return true;
                }
                if(event.code == input::Key::Enter) {
                    if(on_confirm) on_confirm();
                    open = false;
                    return true;
                }
            }
            if(live) (void)body.handle(event);  // Up/Down scroll
            return true;                        // modal: swallow keys
        }

        if(event.kind == input::EventKind::Key &&
           event.code == input::Key::Escape) {
            open = false;
            return true;
        }
        if(event.code == input::Key::Enter) {
            if(on_confirm) on_confirm();
            open = false;
            return true;
        }
        return input.handle(event);
    }

private:
    /// Applies the stored `.look` to this dialog (border, colours, title)
    /// and to its content children. Called by the Look constructors and
    /// by Dialog(DialogFields) / operator=(DialogFields).
    void apply() {
        apply_look(*this, look);
        style_content(look);
    }

    /// Look::fg is the text colour, which the content children own.
    void style_content(const Look& look) {
        Look fg_only{.fg = look.fg};
        apply_look(body, fg_only);
        apply_look(input, fg_only);
    }
};
}  // namespace trux::component
