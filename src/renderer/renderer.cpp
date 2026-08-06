#include "trux/component/component.hpp"
#include "trux/focus/focus_manager.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/layout.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/cell.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/util/util.hpp"

#include <trux/renderer/renderer.hpp>

using namespace trux;

renderer::Renderer::Renderer(layout::Size size) : m_size(size) {
    m_front.resize(size);
    m_back.resize(size);
}

void renderer::Renderer::begin_draw() {
    m_back.clear();
    m_commands.clear();
    m_handlers.clear();
    m_focus.begin_frame();
    m_root = layout::init(m_size);
}

void renderer::Renderer::end_draw() {
    resolve();
    m_batches = build_batches();
    m_focus.end_frame();
}

void renderer::Renderer::commit() { m_front = m_back; }

void renderer::Renderer::resize(layout::Size size) {
    if(size == m_size) return;

    m_size = size;
    m_front.resize(size);
    m_front.invalidate();
    m_back.resize(size);
}
void renderer::Renderer::resize(layout::Region& root) {
    layout::propagate_resize(root, m_size);
}

renderer::CellBuffer& renderer::Renderer::back_buffer() noexcept {
    return m_back;
}

const renderer::CellBuffer& renderer::Renderer::back_buffer() const noexcept {
    return m_back;
}

const renderer::CellBuffer& renderer::Renderer::front_buffer() const noexcept {
    return m_front;
}

renderer::RenderBatches renderer::Renderer::build_batches() const {
    RenderBatches batches;

    auto changes   = m_back.diff(m_front);
    int  running_x = 0;

    for(const auto& change : changes) {
        if(!batches.empty()) {
            auto& batch = batches.back();
            if(change.position.y == batch.position.y &&
               change.position.x == running_x &&
               can_merge(batch.cells.back(), change.cell)) {
                batch.cells.push_back(change.cell);
                running_x += util::glyph_width(change.cell.glyph);
                continue;
            }
        }
        batches.push_back(
            RenderBatch{.position = change.position, .cells = {change.cell}});
        running_x = change.position.x + util::glyph_width(change.cell.glyph);
    }
    return batches;
}

bool renderer::Renderer::can_merge(const Cell& prev, const Cell& next) {
    return prev.foreground == next.foreground &&
           prev.background == next.background && prev.style == next.style;
}

void renderer::Renderer::put(layout::Position pos, Cell cell) {
    if(m_clip && !m_clip->contains(pos)) return;
    if(!m_back.contains(pos)) return;

    auto& dst      = m_back.at(pos);
    dst.background = blend(dst.background, cell.background);
    dst.foreground = blend(dst.foreground, cell.foreground);

    if(cell.glyph != U' ') dst.glyph = cell.glyph;
    dst.style = cell.style;
}

void renderer::Renderer::put(layout::Position pos, char32_t c) {
    put(pos, Cell{.glyph = c});
}

void renderer::Renderer::push(component::ComponentBase& component,
                              layout::Region            area) {
    m_commands.push(SetClip{area});
    component.build(area, m_commands);
    m_commands.push(ClearClip{});

    auto id = static_cast<focus::FocusID>(&component);
    m_focus.register_focusable(id);
    m_handlers.push_back(Handler{id, area, [&component](const input::Event& e) {
                                     return component.handle(e);
                                 }});
}

void renderer::Renderer::text(layout::Region   region,
                              layout::Position pos,
                              std::string_view text) {
    auto current = region.absolute(pos);
    for(char c : text) {
        put(current, static_cast<char32_t>(c));
        current.x++;
    }
}

bool renderer::Renderer::dispatch(const input::Event& e) {
    if(e.kind == input::EventKind::Resize) {
        resize(e.resize);
        return true;
    }
    if(e.kind == input::EventKind::Key && e.code == input::Key::Tab) {
        if(e.mods.has(input::Mod::Shift)) m_focus.focus_prev();
        else m_focus.focus_next();
        return true;
    }

    if(e.kind == input::EventKind::Mouse &&
       e.mouse.kind == input::MouseKind::Press) {
        for(auto it = m_handlers.rbegin(); it != m_handlers.rend(); it++) {
            if(it->region.contains(e.mouse.position)) {
                m_focus.focus(it->id);
                break;
            }
        }
    }

    for(auto it = m_handlers.rbegin(); it != m_handlers.rend(); it++) {
        if(m_focus.is_focused(it->id)) return it->fn(e);
    }
    return false;
}

void renderer::Renderer::resolve() {
    for(const auto& command : m_commands.commands()) {
        std::visit(
            [this](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;

                if constexpr(std::is_same_v<T, DrawText>) {
                    auto             pos  = cmd.position;
                    std::string_view rest = cmd.text;
                    while(auto decoded = util::decode_utf8(rest)) {
                        auto [cp, len] = *decoded;
                        int width      = std::max(1, util::glyph_width(cp));
                        put(pos,
                            Cell{.glyph      = cp,
                                 .foreground = cmd.fg,
                                 .background = cmd.bg,
                                 .style      = cmd.style});
                        for(int i = 1; i < width; i++)
                            put({pos.x + i, pos.y},
                                Cell{.glyph = kContinuationGlyph});
                        pos.x += util::glyph_width(cp);
                        rest.remove_prefix(len);
                    }
                } else if constexpr(std::is_same_v<T, DrawCell>) {
                    put(cmd.position, cmd.cell);
                } else if constexpr(std::is_same_v<T, Fill>) {
                    auto [x, y] = cmd.region.position();
                    auto [w, h] = cmd.region.size();
                    for(int row = 0; row < h; row++) {
                        for(int col = 0; col < w; col++) {
                            put({x + col, y + row}, cmd.cell);
                        }
                    }
                } else if constexpr(std::is_same_v<T, SetClip>) {
                    set_clip(cmd.region);
                } else if constexpr(std::is_same_v<T, ClearClip>) {
                    clear_clip();
                }
            },
            command);
    }
}

void renderer::Renderer::set_clip(layout::Region region) {
    if(!m_clip) {
        m_clip = region;
        return;
    }

    auto a = m_clip->rect();
    auto b = region.rect();

    int x0 = std::max(a.position.x, b.position.x);
    int y0 = std::max(a.position.y, b.position.y);
    int x1 = std::min(a.position.x + a.size.width, b.position.x + b.size.width);
    int y1 =
        std::min(a.position.y + a.size.height, b.position.y + b.size.height);

    m_clip = layout::Region{
        {x0, y0},
        {std::max(0, x1 - x0), std::max(0, y1 - y0)}
    };
}

void renderer::Renderer::clear_clip() { m_clip.reset(); }
