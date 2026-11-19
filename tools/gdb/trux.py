"""gdb support for trux: compact pretty-printers + a screen dump command.

Without this, printing a Renderer/CellBuffer/DrawCommandBuffer expands
every Cell and every DrawCommand (thousands of lines with `set print
pretty on`). With it, those collapse to one-line summaries, and
`trux-screen` shows what is actually on screen as text.

Load:   source /path/to/trux/tools/gdb/trux.py        (e.g. in ~/.gdbinit)
Then:
    print renderer            -> Renderer 80x24 { commands=312 handlers=3 batches=5 }
    print renderer.m_back     -> CellBuffer 80x24
    print cmds                -> DrawCommandBuffer { 312 cmds: DrawCell=290 ... }
    trux-screen renderer      -> the back buffer as a text grid
    trux-screen renderer front [x y w h]
    set trux-limit 20         -> expand the first 20 draw commands inline
    trux-quiet on             -> terser global print settings (trux-quiet off restores)
"""

import re
import struct

import gdb
import gdb.printing

NS = "trux::"

KIND_RGB, KIND_INDEXED, KIND_DEFAULT = 0, 1, 2
INVALID_GLYPH = 0xFFFFFFFF
CONTINUATION_GLYPH = 0xFFFFFFFE


# ── settings ──────────────────────────────────────────────────────────

class TruxLimit(gdb.Parameter):
    """Number of DrawCommands to expand inline when printing a DrawCommandBuffer.
0 (default) prints only the summary."""
    set_doc = "Set how many draw commands trux printers expand."
    show_doc = "Show how many draw commands trux printers expand."

    def __init__(self):
        super().__init__("trux-limit", gdb.COMMAND_DATA, gdb.PARAM_ZUINTEGER)
        self.value = 0

    def get_set_string(self):
        return ""

    def get_show_string(self, svalue):
        return "trux draw-command expansion limit is " + svalue


_limit = TruxLimit()


# ── helpers ───────────────────────────────────────────────────────────

def _int(v):
    return int(v)


def _glyph(cp):
    if cp == INVALID_GLYPH:
        return "<invalid>"
    if cp == CONTINUATION_GLYPH:
        return "<cont>"
    if cp < 0x20 or cp == 0x7F:
        return "U+%04X" % cp
    try:
        return repr(chr(cp))
    except (ValueError, OverflowError):
        return "U+%X" % cp


def _color(v):
    kind = _int(v["kind"])
    a = _int(v["a"])
    if kind == KIND_INDEXED:
        return "ansi(%d)" % _int(v["index"])
    if kind == KIND_DEFAULT:
        return "default"
    r, g, b = _int(v["r"]), _int(v["g"]), _int(v["b"])
    s = "#%02x%02x%02x" % (r, g, b)
    return s if a == 255 else "%s/%d" % (s, a)


STYLE_BITS = [(1, "bold"), (2, "italic"), (4, "underline"), (8, "blink"),
              (16, "reverse"), (32, "dim"), (64, "strike")]


def _style(v):
    bits = _int(v)
    return "+".join(n for b, n in STYLE_BITS if bits & b)


FLAG_BITS = [(1 << 0, "BorderSingle"), (1 << 1, "BorderRounded"),
             (1 << 2, "BorderDouble"), (1 << 3, "BorderBlock"),
             (1 << 4, "Bold"), (1 << 5, "Italic"), (1 << 6, "Underline")]


def _pos(v):
    return "(%d,%d)" % (_int(v["x"]), _int(v["y"]))


def _size(v):
    return "%dx%d" % (_int(v["width"]), _int(v["height"]))


def _rect(v):
    return "%s %s" % (_pos(v["position"]), _size(v["size"]))


def _sv(v):
    """Text of a std::string_view value, truncated for display."""
    try:
        n = _int(v["_M_len"])
        s = v["_M_str"].string(length=min(n, 60))
        return repr(s) + ("..." if n > 60 else "")
    except gdb.error:
        return "<unreadable>"


def _cell_text(v):
    out = _glyph(_int(v["glyph"]))
    out += " fg=" + _color(v["foreground"])
    if _int(v["background"]["a"]) > 0:
        out += " bg=" + _color(v["background"])
    st = _style(v["style"])
    if st:
        out += " " + st
    return out


def _utf8_host():
    try:
        return gdb.host_charset().lower().replace("-", "") in ("utf8", "")
    except Exception:
        return False


def _display(cp):
    """One screen glyph. Falls back to ASCII when gdb's host charset can't
    encode it (e.g. LANG=C), so the command never dies on a border glyph."""
    if cp == 0x20:
        return "."
    if _utf8_host():
        return chr(cp)
    if cp < 0x80:
        return chr(cp)
    if 0x2500 <= cp <= 0x257F:
        if cp in (0x2500, 0x2550):
            return "-"
        if cp in (0x2502, 0x2551):
            return "|"
        return "+"
    return "#"


# ── printers ──────────────────────────────────────────────────────────

class ColorPrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return _color(self.v)


class CellPrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return "Cell{%s}" % _cell_text(self.v)


class PositionPrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return _pos(self.v)


class SizePrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return _size(self.v)


class RectPrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return "Rect{%s}" % _rect(self.v)


def _region_rect(region):
    """Live rect lives in the shared RegionNode; m_rect is a stale copy."""
    try:
        return region["m_node"]["_M_ptr"].dereference()["rect"]
    except gdb.error:
        return region["m_rect"]


class RegionPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        try:
            return "Region{%s}" % _rect(_region_rect(self.v))
        except gdb.error:
            return "Region{?}"


class FlagsPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        bits = _int(self.v["value"])
        names = [n for b, n in FLAG_BITS if bits & b]
        return "Flags(%s)" % ("|".join(names) if names else "none")


class DrawTextPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        v = self.v
        return "DrawText %s %s fg=%s" % (_pos(v["position"]), _sv(v["text"]),
                                         _color(v["fg"]))


class DrawCellPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        return "DrawCell %s %s" % (_pos(self.v["position"]), _cell_text(self.v["cell"]))


class FillPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        return "Fill %s %s" % (_rect(_region_rect(self.v["region"])), _cell_text(self.v["cell"]))


class SetClipPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        return "SetClip %s%s" % (_rect(_region_rect(self.v["region"])),
                                 " modal" if _int(self.v["modal"]) else "")


class ClearClipPrinter:
    def __init__(self, v): self.v = v
    def to_string(self): return "ClearClip"


def _variant_alt(elem):
    """(index, alt_value) of a libstdc++ std::variant, or (None, None)."""
    try:
        idx = _int(elem["_M_index"])
        t = elem.type.strip_typedefs()
        u = elem["_M_u"]
        for _ in range(idx):
            u = u["_M_rest"]
        # Count template args to find the alternative's type.
        i, alt_t = 0, None
        while True:
            try:
                a = t.template_argument(i)
            except Exception:
                break
            if i == idx:
                alt_t = a
                break
            i += 1
        if alt_t is None:
            return None, None
        return idx, u["_M_first"].address.cast(alt_t.pointer()).dereference()
    except gdb.error:
        return None, None


def _variant_names(vec_val):
    t = vec_val.type.strip_typedefs().template_argument(0).strip_typedefs()
    names, i = [], 0
    while True:
        try:
            a = t.template_argument(i)
        except Exception:
            break
        names.append(str(a).split("::")[-1])
        i += 1
    return names


def _vector_range(v):
    impl = v["_M_impl"]
    return impl["_M_start"], impl["_M_finish"]


class DrawCommandBufferPrinter:
    def __init__(self, v): self.v = v

    def _summarize(self, vec):
        start, finish = _vector_range(vec)
        n = _int(finish - start)
        counts = {}
        if n:
            names = _variant_names(vec)
            for i in range(n):
                idx = _int(start[i]["_M_index"])
                nm = names[idx] if idx < len(names) else "?"
                counts[nm] = counts.get(nm, 0) + 1
        return n, counts

    def to_string(self):
        n, counts = self._summarize(self.v["m_commands"])
        on, ocounts = self._summarize(self.v["m_overlay_commands"])
        parts = ", ".join("%s=%d" % kv for kv in sorted(counts.items(), key=lambda kv: -kv[1]))
        s = "DrawCommandBuffer{ %d cmds" % n
        if parts:
            s += ": " + parts
        if on:
            s += " | overlay %d" % on
        s += " }"
        lim = _int(_limit.value)
        if lim == 0 and n:
            s += "  (set trux-limit N to expand)"
        return s

    def display_hint(self):
        return "array"

    def children(self):
        lim = _int(_limit.value)
        if lim == 0:
            return
        start, finish = _vector_range(self.v["m_commands"])
        n = min(_int(finish - start), lim)
        for i in range(n):
            _, alt = _variant_alt(start[i])
            yield "[%d]" % i, (alt if alt is not None else start[i])


class RenderBatchPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        start, finish = _vector_range(self.v["cells"])
        n = _int(finish - start)
        text = []
        for i in range(min(n, 40)):
            cp = _int(start[i]["glyph"])
            if cp != CONTINUATION_GLYPH:
                text.append(_display(cp) if 0x20 <= cp <= 0x10FFFF else "?")
        return "Batch %s %d cells |%s%s|" % (_pos(self.v["position"]), n,
                                             "".join(text), "+" if n > 40 else "")


class CellBufferPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        return "CellBuffer %s" % _size(self.v["m_size"])


class RendererPrinter:
    def __init__(self, v): self.v = v

    def to_string(self):
        v = self.v
        try:
            cmds = _int(_vector_range(v["m_commands"]["m_commands"])[1] -
                        _vector_range(v["m_commands"]["m_commands"])[0])
            hs = _int(_vector_range(v["m_handlers"])[1] - _vector_range(v["m_handlers"])[0])
            bs = _int(_vector_range(v["m_batches"])[1] - _vector_range(v["m_batches"])[0])
            return "Renderer %s { commands=%d handlers=%d batches=%d }" % (
                _size(v["m_size"]), cmds, hs, bs)
        except gdb.error:
            return "Renderer %s" % _size(v["m_size"])


def build_pretty_printers():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("trux")
    add = pp.add_printer
    add("Color", r"^trux::style::Color$", ColorPrinter)
    add("Cell", r"^trux::renderer::Cell$", CellPrinter)
    add("Position", r"^trux::layout::Position$", PositionPrinter)
    add("Size", r"^trux::layout::Size$", SizePrinter)
    add("Rect", r"^trux::layout::Rect$", RectPrinter)
    add("Region", r"^trux::layout::Region$", RegionPrinter)
    add("ComponentFlags", r"^trux::component::ComponentFlags$", FlagsPrinter)
    add("DrawText", r"^trux::renderer::DrawText$", DrawTextPrinter)
    add("DrawCell", r"^trux::renderer::DrawCell$", DrawCellPrinter)
    add("Fill", r"^trux::renderer::Fill$", FillPrinter)
    add("SetClip", r"^trux::renderer::SetClip$", SetClipPrinter)
    add("ClearClip", r"^trux::renderer::ClearClip$", ClearClipPrinter)
    add("DrawCommandBuffer", r"^trux::renderer::DrawCommandBuffer$", DrawCommandBufferPrinter)
    add("RenderBatch", r"^trux::renderer::RenderBatch$", RenderBatchPrinter)
    add("CellBuffer", r"^trux::renderer::CellBuffer$", CellBufferPrinter)
    add("Renderer", r"^trux::renderer::Renderer$", RendererPrinter)
    return pp


# ── commands ──────────────────────────────────────────────────────────

class TruxScreen(gdb.Command):
    """Dump a CellBuffer (or a Renderer's back/front buffer) as text.

Usage: trux-screen EXPR [front|back] [X Y W H]

Glyphs only, no colors. '.' marks an empty cell, '?' an invalidated one.
Wide-glyph continuation cells are skipped so columns line up."""

    def __init__(self):
        super().__init__("trux-screen", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if not argv:
            raise gdb.GdbError("usage: trux-screen EXPR [front|back] [X Y W H]")
        val = gdb.parse_and_eval(argv[0])
        which, rest = "back", argv[1:]
        if rest and rest[0] in ("front", "back"):
            which, rest = rest[0], rest[1:]
        t = val.type.strip_typedefs()
        if t.code == gdb.TYPE_CODE_REF:
            val, t = val.referenced_value(), val.referenced_value().type.strip_typedefs()
        if t.code == gdb.TYPE_CODE_PTR:
            val = val.dereference()
            t = val.type.strip_typedefs()
        if str(t).endswith("Renderer"):
            val = val["m_front" if which == "front" else "m_back"]
        w, h = _int(val["m_size"]["width"]), _int(val["m_size"]["height"])
        x0 = y0 = 0
        cw, ch = w, h
        if len(rest) == 4:
            x0, y0, cw, ch = (int(x) for x in rest)
        cell_t = gdb.lookup_type("trux::renderer::Cell")
        stride = cell_t.sizeof
        goff = cell_t["glyph"].bitpos // 8
        start, finish = _vector_range(val["m_cells"])
        if w <= 0 or h <= 0 or _int(finish - start) < w * h:
            raise gdb.GdbError("buffer is empty or inconsistent (%dx%d)" % (w, h))
        raw = bytes(gdb.selected_inferior().read_memory(int(start), w * h * stride))
        lines = ["   " + "".join(str((x // 10) % 10) if x % 10 == 0 else " "
                                  for x in range(x0, min(x0 + cw, w)))]
        for y in range(y0, min(y0 + ch, h)):
            row = []
            for x in range(x0, min(x0 + cw, w)):
                cp = struct.unpack_from("<I", raw, (y * w + x) * stride + goff)[0]
                if cp == CONTINUATION_GLYPH:
                    continue
                if cp == INVALID_GLYPH:
                    row.append("?")
                elif cp != 0x20 and (cp < 0x20 or cp > 0x10FFFF):
                    row.append("?")
                else:
                    row.append(_display(cp))
            lines.append("%2d %s" % (y, "".join(row)))
        gdb.write("\n".join(lines) + "\n")


class TruxQuiet(gdb.Command):
    """trux-quiet [on|off]: terser global print settings for debugging TUIs.

on:  print elements 24, repeats 8, max-depth 3, frame-arguments scalars,
     and skip libstdc++ internals when stepping.
off: restore gdb defaults for those settings."""

    def __init__(self):
        super().__init__("trux-quiet", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        on = arg.strip() != "off"
        cmds = (["set print elements 24", "set print repeats 8",
                 "set print max-depth 3", "set print frame-arguments scalars",
                 "skip -gfi /usr/include/c++/*/*", "skip -gfi */trux/diag/*"]
                if on else
                ["set print elements 200", "set print repeats 10",
                 "set print max-depth 20", "set print frame-arguments scalars",
                 "delete skip"])
        for c in cmds:
            try:
                gdb.execute(c, to_string=True)
            except gdb.error as e:
                gdb.write("trux-quiet: %s: %s\n" % (c, e))
        gdb.write("trux-quiet %s\n" % ("on" if on else "off"))


def register():
    gdb.printing.register_pretty_printer(gdb.current_objfile(), build_pretty_printers(),
                                         replace=True)
    TruxScreen()
    TruxQuiet()


register()
