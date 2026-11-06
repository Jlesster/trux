#include "trux/component/component.hpp"
#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <trux/layout/region.hpp>
#include <vector>

using namespace trux;

namespace {
std::vector<std::weak_ptr<layout::RegionNode>>& root_registry() {
    static std::vector<std::weak_ptr<layout::RegionNode>> registry;
    return registry;
}

int resolve_fixed(int cells, int parent) {
    int value = cells < 0 ? parent + cells : cells;
    return std::clamp(value, 0, parent);
}
}  // namespace

struct layout::RegionNode {
    Rect rect;
    std::map<SplitKey, std::shared_ptr<Split>> children;
    std::shared_ptr<component::ComponentBase> component;
    std::shared_ptr<Split> active_split;
    std::weak_ptr<RegionNode>
        parent;  // weak: children->split->node->parent was a cycle
    SizeRequest request;
};

layout::Region::Region(Position pos, Size size)
    : m_node(
          std::make_shared<RegionNode>(RegionNode{Rect{pos, size}, {}, {}})) {}
layout::Region::Region()
    : m_node(std::make_shared<RegionNode>()) {}

layout::Size layout::Region::size() const noexcept { return m_node->rect.size; }
layout::Rect layout::Region::rect() const noexcept { return m_node->rect; }

layout::Rect layout::Region::root_rect() const noexcept {
    auto node = m_node;
    while(auto p = node->parent.lock()) node = std::move(p);
    return node->rect;
}

layout::Position layout::Region::position() const noexcept {
    return m_node->rect.position;
}

void layout::Region::set_component(
    std::shared_ptr<component::ComponentBase> c) {
    m_node->component = std::move(c);
    if(m_node->request != SizeRequest{}) request_size({});
}

void layout::Region::layout_split(RegionNode& parent, const SplitKey& key,
                                  Split& split) {
    const bool vertical = key.orientation == Orientation::Vertical;
    const Rect& p = parent.rect;
    const int along = vertical ? p.size.width : p.size.height;
    const int across = vertical ? p.size.height : p.size.width;
    const int overlap = key.shared ? 1 : 0;

    const SizeRequest& fr = split.first.m_node->request;
    const SizeRequest& sr = split.second.m_node->request;
    auto along_of = [&](const SizeRequest& r) {
        return vertical ? r.width : r.height;
    };
    auto across_of = [&](const SizeRequest& r) {
        return vertical ? r.height : r.width;
    };

    // Natural first extent, identical to what the *_split calls use.
    int first =
        key.fixed ? resolve_fixed(key.value, along) : along * key.value / 100;
    if(auto w = along_of(fr)) {
        first = std::clamp(*w, 0, along);
    } else if(auto w = along_of(sr)) {
        first =
            std::clamp(along + overlap - std::clamp(*w, 0, along), 0, along);
    }
    // A fully-swallowed sibling gets no overlap sliver either.
    const int second = first >= along ? 0 : along - first + overlap;
    const int second_start = first >= along ? along : first - overlap;

    const int first_across =
        std::clamp(across_of(fr).value_or(across), 0, across);
    const int second_across =
        std::clamp(across_of(sr).value_or(across), 0, across);

    auto make = [&](int start, int extent, int cross) {
        return vertical
                   ? Rect{{p.position.x + start, p.position.y}, {extent, cross}}
                   : Rect{{p.position.x, p.position.y + start},
                          {cross, extent}};
    };
    split.first.m_node->rect = make(0, first, first_across);
    split.second.m_node->rect = make(second_start, second, second_across);
}

void layout::Region::reflow(RegionNode& node) {
    for(auto& [key, split] : node.children) {
        layout_split(node, key, *split);
        reflow(*split->first.m_node);
        reflow(*split->second.m_node);
    }
}

void layout::Region::request_size(SizeRequest request) const {
    m_node->request = request;
    auto parent = m_node->parent.lock();
    if(!parent) return;
    for(auto& [key, split] : parent->children) {
        if(split->first.m_node != m_node && split->second.m_node != m_node)
            continue;
        layout_split(*parent, key, *split);
        reflow(*split->first.m_node);
        reflow(*split->second.m_node);
        return;
    }
}

void layout::Region::build(renderer::DrawCommandBuffer& cmd) const {
    if(m_node->component) m_node->component->build(*this, cmd);
    else if(m_node->active_split) m_node->active_split->build(*this, cmd);
}

void layout::Split::build(Region /*area*/,
                          renderer::DrawCommandBuffer& cmd) const {
    first.build(cmd);
    second.build(cmd);
}

layout::Region layout::init(Size size) {
    Region root{{0, 0}, size};
    root_registry().push_back(root.m_node);
    return root;
}

void layout::resize_all_roots(Size new_size) {
    auto& registry = root_registry();
    std::erase_if(registry, [](const auto& w) { return w.expired(); });
    for(auto& weak : registry)
        if(auto node = weak.lock()) {
            Region r{node};
            propagate_resize(r, new_size);
        }
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

component::ComponentBase* layout::Region::component_ptr() const noexcept {
    return m_node->component.get();
}

layout::Split& layout::Region::v_split(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Vertical, false, percent};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_width = m_node->rect.size.width * percent / 100;

    auto first =
        Region{m_node->rect.position, {first_width, m_node->rect.size.height}};
    auto second = Region{
        {m_node->rect.position.x + first_width, m_node->rect.position.y},
        {m_node->rect.size.width - first_width, m_node->rect.size.height}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

layout::Split& layout::Region::h_split(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Horizontal, false, percent};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_height = m_node->rect.size.height * percent / 100;

    auto first =
        Region{m_node->rect.position, {m_node->rect.size.width, first_height}};
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + first_height},
        {m_node->rect.size.width, m_node->rect.size.height - first_height}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

layout::Split& layout::Region::v_split_fixed(int cells) const {
    SplitKey key{Orientation::Vertical, true, cells};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_height = resolve_fixed(cells, m_node->rect.size.width);
    auto first =
        Region{m_node->rect.position, {first_height, m_node->rect.size.height}};
    auto second = Region{
        {m_node->rect.position.x + first_height, m_node->rect.position.y},
        {m_node->rect.size.width - first_height, m_node->rect.size.height}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

layout::Split& layout::Region::h_split_fixed(int cells) const {
    SplitKey key{Orientation::Horizontal, true, cells};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_height = resolve_fixed(cells, m_node->rect.size.height);
    auto first =
        Region{m_node->rect.position, {m_node->rect.size.width, first_height}};
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + first_height},
        {m_node->rect.size.width, m_node->rect.size.height - first_height}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

layout::Split& layout::Region::v_split_shared(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Vertical, false, percent, true};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_width = m_node->rect.size.width * percent / 100;
    auto first =
        Region{m_node->rect.position, {first_width, m_node->rect.size.height}};
    auto second = Region{
        {m_node->rect.position.x + first_width - 1, m_node->rect.position.y},
        {m_node->rect.size.width - first_width + 1, m_node->rect.size.height}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

layout::Split& layout::Region::h_split_shared(int percent) const {
    percent = std::clamp(percent, 0, 100);
    SplitKey key{Orientation::Horizontal, false, percent, true};
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    int first_height = m_node->rect.size.height * percent / 100;
    auto first =
        Region{m_node->rect.position, {m_node->rect.size.width, first_height}};
    auto second = Region{
        {m_node->rect.position.x, m_node->rect.position.y + first_height - 1},
        {m_node->rect.size.width, m_node->rect.size.height - first_height + 1}};
    first.m_node->parent = m_node;
    second.m_node->parent = m_node;

    auto [it, _] = m_node->children.emplace(
        key, std::make_shared<Split>(Split{first, second}));
    m_node->active_split = it->second;
    return *it->second;
}

void layout::propagate_resize(layout::Region& region, layout::Size new_size) {
    region.m_node->rect = Rect{{0, 0}, new_size};
    Region::reflow(*region.m_node);
}

void layout::propagate_resize(Size new_size) { resize_all_roots(new_size); }
