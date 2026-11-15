#include "trux/component/component.hpp"
#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <trux/layout/region.hpp>
#include <unordered_map>
#include <vector>

using namespace trux;

namespace {
std::vector<std::weak_ptr<layout::RegionNode>>& root_registry() {
    static std::vector<std::weak_ptr<layout::RegionNode>> registry;
    return registry;
}

struct SvHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

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
    std::unordered_map<std::string, layout::Region, SvHash, std::equal_to<>>
        named;  // populated on the root node only
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
    const int n = static_cast<int>(split.parts.size());

    auto along_of = [&](const SizeRequest& r) {
        return vertical ? r.width : r.height;
    };
    auto across_of = [&](const SizeRequest& r) {
        return vertical ? r.height : r.width;
    };

    // Natural extents (seam overlap not included). Legacy keys size only the
    // first part (from `value`); keys with `sizes` size each part that has
    // an entry. Unsized parts share what is left equally, earliest first.
    std::vector<std::optional<int>> spec(n);
    if(key.sizes.empty()) spec[0] = key.value;
    else
        for(int i = 0; i < n && i < static_cast<int>(key.sizes.size()); ++i)
            spec[i] = key.sizes[i];

    std::vector<int> nat(n, 0);
    int used = 0, unset = 0;
    for(int i = 0; i < n; ++i) {
        if(!spec[i]) {
            ++unset;
            continue;
        }
        const int want =
            key.fixed ? resolve_fixed(*spec[i], along) : along * *spec[i] / 100;
        nat[i] = std::min(want, along - used);  // never overdraw
        used += nat[i];
    }
    const int rest = along - used;
    if(unset == 0) nat[n - 1] += rest;  // all explicit: last absorbs
    else {
        int k = 0;
        for(int i = 0; i < n; ++i)
            if(!spec[i]) nat[i] = rest / unset + (k++ < rest % unset ? 1 : 0);
    }

    // Parts that asked for an along-axis size are pinned to it. A part's
    // visible extent includes the seam overlap it reaches back over,
    // so strip that to get its share of `along`.
    std::vector<std::optional<int>> pinned(n);
    for(int i = 0; i < n; ++i)
        if(auto w = along_of(split.parts[i].m_node->request))
            pinned[i] = std::clamp(*w, 0, along) - (i > 0 ? overlap : 0);
    // If everyone is pinned the last part absorbs the difference.
    if(std::ranges::all_of(pinned, [](auto& o) { return o.has_value(); }))
        pinned[n - 1].reset();

    std::vector<int> ext = nat;
    int free_nat = 0;
    for(int i = 0; i < n; ++i)
        if(!pinned[i]) free_nat += nat[i];
    // Pinned parts are honoured in order while room remains.
    int room = along;
    for(int i = 0; i < n; ++i)
        if(pinned[i]) {
            ext[i] = std::clamp(*pinned[i], 0, room);
            room -= ext[i];
        }
    // Unpinned parts split `room` in proportion to their natural size
    // (equally if they are all naturally empty); the last one takes the
    // rounding remainder.
    int free_count = 0, last_free = -1;
    for(int i = 0; i < n; ++i)
        if(!pinned[i]) {
            ++free_count;
            last_free = i;
        }
    int left = room;
    for(int i = 0; i < n; ++i) {
        if(pinned[i]) continue;
        if(i == last_free) {
            ext[i] = left;
            break;
        }
        int share = free_nat > 0
                        ? static_cast<int>(static_cast<long long>(room) *
                                           nat[i] / free_nat)
                        : room / free_count;
        ext[i] = share;
        left -= share;
    }

    auto make = [&](int start, int extent, int cross) {
        return vertical
                   ? Rect{{p.position.x + start, p.position.y}, {extent, cross}}
                   : Rect{{p.position.x, p.position.y + start},
                          {cross, extent}};
    };
    int cursor = 0;
    for(int i = 0; i < n; ++i) {
        auto& node = *split.parts[i].m_node;
        const int cross =
            std::clamp(across_of(node.request).value_or(across), 0, across);
        // Parts after the first reach back over the seam; a fully
        // swallowed part gets no overlap sliver either.
        const bool reach = i > 0 && ext[i] > 0;
        const int start = reach ? cursor - overlap : cursor;
        const int extent = ext[i] + (reach ? overlap : 0);
        node.rect = make(start, extent, cross);
        cursor += ext[i];
    }
}

void layout::Region::reflow(RegionNode& node) {
    for(auto& [key, split] : node.children) {
        layout_split(node, key, *split);
        for(auto& part : split->parts) reflow(*part.m_node);
    }
}

void layout::Region::request_size(SizeRequest request) const {
    m_node->request = request;
    auto parent = m_node->parent.lock();
    if(!parent) return;
    for(auto& [key, split] : parent->children) {
        if(std::ranges::none_of(split->parts, [&](const Region& r) {
               return r.m_node == m_node;
           }))
            continue;
        layout_split(*parent, key, *split);
        for(auto& part : split->parts) reflow(*part.m_node);
        return;
    }
}

void layout::Region::build(renderer::DrawCommandBuffer& cmd) const {
    if(m_node->component) m_node->component->build(*this, cmd);
    else if(m_node->active_split) m_node->active_split->build(*this, cmd);
}

void layout::Split::build(Region /*area*/,
                          renderer::DrawCommandBuffer& cmd) const {
    for(const Region& part : parts) part.build(cmd);
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

layout::Split& layout::Region::split_impl(
    const SplitKey& key, std::shared_ptr<Split> (*make)()) const {
    if(auto it = m_node->children.find(key); it != m_node->children.end()) {
        m_node->active_split = it->second;
        return *it->second;
    }

    auto split = make();
    split->parts.reserve(key.parts);
    for(int i = 0; i < key.parts; ++i) {
        Region child;
        child.m_node->parent = m_node;
        split->parts.push_back(std::move(child));
    }
    layout_split(*m_node, key, *split);

    auto [it, _] = m_node->children.emplace(key, std::move(split));
    m_node->active_split = it->second;
    return *it->second;
}

void layout::propagate_resize(layout::Region& region, layout::Size new_size) {
    region.m_node->rect = Rect{{0, 0}, new_size};
    Region::reflow(*region.m_node);
}

void layout::propagate_resize(Size new_size) { resize_all_roots(new_size); }

namespace {
std::shared_ptr<layout::RegionNode> root_of(
    std::shared_ptr<layout::RegionNode> n) {
    while(auto p = n->parent.lock()) n = std::move(p);
    return n;
}
}  // namespace

void layout::Region::set_name(std::string_view name) const {
    root_of(m_node)->named.insert_or_assign(std::string(name), *this);
}

layout::Leaves layout::Region::leaf() const {
    return Leaves{Region{root_of(m_node)}};
}

layout::Region* layout::Leaves::find(std::string_view name) const {
    if(name == "root") return const_cast<Region*>(&m_root);
    auto& named = m_root.m_node->named;
    auto it = named.find(name);
    return it == named.end() ? nullptr : &it->second;
}

layout::Region& layout::Leaves::operator[](std::string_view name) const {
    if(auto* r = find(name)) return *r;
    throw std::out_of_range("trux: no leaf named \"" + std::string(name) +
                            "\"");
}

bool layout::Leaves::contains(std::string_view name) const {
    return find(name) != nullptr;
}

bool layout::Leaves::is_leaf(std::string_view name) const {
    auto* r = find(name);
    return r && !r->m_node->active_split;
}

std::size_t layout::Leaves::size() const { return m_root.m_node->named.size(); }

std::vector<std::string> layout::Leaves::names() const {
    std::vector<std::string> out;
    for(auto& [k, _] : m_root.m_node->named) out.push_back(k);
    std::ranges::sort(out);
    return out;
}

void layout::Leaves::each(
    const std::function<void(std::string_view, Region&)>& fn) const {
    for(auto& [k, v] : m_root.m_node->named) fn(k, v);
}

bool layout::Leaves::erase(std::string_view name) const {
    auto& named = m_root.m_node->named;
    auto it = named.find(name);
    if(it == named.end()) return false;
    named.erase(it);
    return true;
}

void layout::Leaves::clear() const { m_root.m_node->named.clear(); }
