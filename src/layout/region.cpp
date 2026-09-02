#include "trux/component/component.hpp"
#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <trux/layout/region.hpp>

using namespace trux;

struct layout::RegionNode {
    Rect                                       rect;
    std::map<SplitKey, std::shared_ptr<Split>> children;
    std::shared_ptr<component::ComponentBase>  component;
};

layout::Region::Region(Position pos, Size size)
    : m_node(std::make_shared<RegionNode>(RegionNode{
          Rect{pos, size},
          {},
          {}
})) {}

layout::Size layout::Region::size() const noexcept { return m_node->rect.size; }
layout::Rect layout::Region::rect() const noexcept { return m_node->rect; }

layout::Position layout::Region::position() const noexcept {
    return m_node->rect.position;
}

void layout::Region::set_component(
    std::shared_ptr<component::ComponentBase> c) {
    m_node->component = std::move(c);
}

void layout::Region::build(renderer::DrawCommandBuffer& cmd) const {
    if(m_node->component) m_node->component->build(*this, cmd);
}

void layout::Split::build(Region /*area*/,
                          renderer::DrawCommandBuffer& cmd) const {
    first.build(cmd);
    second.build(cmd);
}

bool layout::Region::contains(Position pos) const noexcept {
    auto& r = m_node->rect;
    return pos.x >= r.position.x && pos.y >= r.position.y &&
           pos.x < r.position.x + r.size.width &&
           pos.y < r.position.y + r.size.height;
}

layout::Position layout::Region::absolute(Position local) const noexcept {
    return {m_node->rect.position.x + local.x,
            m_node->rect.position.y + local.y};
}

layout::Split& layout::Region::v_split(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Vertical, false, percent};
    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    int first_width = m_node->rect.size.width * percent / 100;

    auto first = Region{
        m_node->rect.position, {first_width, m_node->rect.size.height}
    };
    auto second = Region{
        {m_node->rect.position.x + first_width, m_node->rect.position.y },
        {m_node->rect.size.width - first_width, m_node->rect.size.height}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

layout::Split& layout::Region::h_split(int percent) const {
    percent = std::clamp(percent, 0, 100);

    SplitKey key{Orientation::Horizontal, false, percent};

    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    int first_height = m_node->rect.size.height * percent / 100;

    auto first = Region{
        m_node->rect.position, {m_node->rect.size.width, first_height}
    };
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + first_height },
        {m_node->rect.size.width, m_node->rect.size.height - first_height}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

layout::Split& layout::Region::v_split_fixed(int cells) const {
    cells = std::clamp(cells, 0, size().width);
    SplitKey key{Orientation::Vertical, true, cells};

    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    auto first = Region{
        m_node->rect.position, {cells, m_node->rect.size.height}
    };
    auto second = Region{
        {m_node->rect.position.x + cells, m_node->rect.position.y },
        {m_node->rect.size.width - cells, m_node->rect.size.height}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

layout::Split& layout::Region::h_split_fixed(int cells) const {
    cells = std::clamp(cells, 0, size().height);
    SplitKey key{Orientation::Horizontal, true, cells};

    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    auto first = Region{
        m_node->rect.position, {cells, m_node->rect.size.height}
    };
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + cells },
        {m_node->rect.size.width, m_node->rect.size.height - cells}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

layout::Split& layout::Region::v_split_shared(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Vertical, false, percent, true};
    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    int  first_width = m_node->rect.size.width * percent / 100;
    auto first       = Region{
        m_node->rect.position, {first_width, m_node->rect.size.height}
    };
    auto second = Region{
        {m_node->rect.position.x + first_width - 1, m_node->rect.position.y },
        {m_node->rect.size.width - first_width + 1, m_node->rect.size.height}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

layout::Split& layout::Region::h_split_shared(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Horizontal, false, percent, true};
    if(auto it = m_node->children.find(key); it != m_node->children.end())
        return *it->second;

    int  first_height = m_node->rect.size.height * percent / 100;
    auto first        = Region{
        m_node->rect.position, {m_node->rect.size.width, first_height}
    };
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + first_height - 1 },
        {m_node->rect.size.width, m_node->rect.size.height - first_height + 1}
    };

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    return *it->second;
}

void layout::propagate_resize(layout::Region& region, layout::Size new_size) {
    region.m_node->rect = Rect{
        {0, 0},
        new_size
    };

    std::function<void(RegionNode&)> resize_children = [&](RegionNode& node) {
        for(auto& [key, split] : node.children) {
            auto& parent_rect = node.rect;
            int value = key.fixed
                            ? key.value
                            : (key.orientation == Orientation::Vertical
                                   ? parent_rect.size.width * key.value / 100
                                   : parent_rect.size.height * key.value / 100);
            int overlap = key.shared ? 1 : 0;

            if(key.orientation == Orientation::Vertical) {
                split->first.m_node->rect = {
                    parent_rect.position, {value, parent_rect.size.height}
                };
                split->second.m_node->rect = {
                    {parent_rect.position.x + value - overlap,
                     parent_rect.position.y },
                    {parent_rect.size.width - value + overlap,
                     parent_rect.size.height}
                };
            } else {
                split->first.m_node->rect = {
                    parent_rect.position, {parent_rect.size.width, value}
                };
                split->second.m_node->rect = {
                    {parent_rect.position.x,
                     parent_rect.position.y + value - overlap },
                    {parent_rect.size.width,
                     parent_rect.size.height - value + overlap}
                };
            }
            resize_children(*split->first.m_node);
            resize_children(*split->second.m_node);
        }
    };
    resize_children(*region.m_node);
}
