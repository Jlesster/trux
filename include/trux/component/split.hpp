#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <trux/component/component.hpp>
#include <trux/layout/region.hpp>
#include <utility>
#include <vector>

namespace trux::component {

struct Split {
    ComponentFlags flags{};

    using KeyPredicate = std::function<bool(const input::Event&)>;

    template <ComponentType T> explicit Split(T initial) {
        m_root.leaf = std::make_unique<ComponentWrapper<T>>(std::move(initial));
        m_root.active_here = true;
    }

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        build_node(m_root, area, cmd);
    }

    void set_focus_next_key(KeyPredicate pred) {
        m_next_pred = std::move(pred);
    }
    void set_focus_prev_key(KeyPredicate pred) {
        m_prev_pred = std::move(pred);
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(m_next_pred && m_next_pred(event)) {
            focus_next();
            return true;
        }
        if(m_prev_pred && m_prev_pred(event)) {
            focus_prev();
            return true;
        }
        auto* active = find_active(m_root);
        return active && active->leaf ? active->leaf->handle(event) : false;
    }

    void focus_next() { move_active(1); }
    void focus_prev() { move_active(-1); }

    template <ComponentType T> void split_v(T content) {
        split(layout::Axis::Vertical, std::move(content));
    }
    template <ComponentType T> void split_h(T content) {
        split(layout::Axis::Horizontal, std::move(content));
    }

    void close_active() {
        auto* active = find_active(m_root);
        if(!active) return;
        auto* parent = find_parent(m_root, active);
        if(!parent) return;

        auto& survivor_ptr =
            (parent->first.get() == active) ? parent->second : parent->first;

        Node promoted = std::move(*survivor_ptr);
        *parent       = std::move(promoted);

        clear_active(m_root);
        if(auto* leaf = leftmost_leaf(*parent)) leaf->active_here = true;
    }

private:
    struct Node {
        std::unique_ptr<ComponentBase> leaf;
        layout::Axis                   axis{};
        float                          ratio{0.5f};
        std::unique_ptr<Node>          first, second;
        bool                           active_here{false};

        [[nodiscard]]
        bool is_leaf() const noexcept {
            return leaf != nullptr;
        }
    };

    Node         m_root;
    KeyPredicate m_next_pred;
    KeyPredicate m_prev_pred;

    template <ComponentType T> void split(layout::Axis axis, T content) {
        auto* target = find_active(m_root);
        if(!target || !target->is_leaf()) return;

        auto first         = std::make_unique<Node>();
        first->leaf        = std::move(target->leaf);
        first->active_here = false;

        auto second = std::make_unique<Node>();
        second->leaf =
            std::make_unique<ComponentWrapper<T>>(std::move(content));
        second->active_here = true;

        target->leaf   = nullptr;
        target->axis   = axis;
        target->ratio  = 0.5f;
        target->first  = std::move(first);
        target->second = std::move(second);
    }

    void build_node(const Node&                  n,
                    layout::Region               area,
                    renderer::DrawCommandBuffer& cmd) const {
        if(n.is_leaf()) {
            n.leaf->build(area, cmd);
            return;
        }
        int   percent = static_cast<int>(n.ratio * 100.0f);
        auto& s = (n.axis == layout::Axis::Vertical) ? area.v_split(percent)
                                                     : area.h_split(percent);

        build_node(*n.first, s[0], cmd);
        build_node(*n.second, s[1], cmd);
    }

    static Node* find_active(Node& n) {
        if(n.is_leaf()) return n.active_here ? &n : nullptr;
        if(auto* f = find_active(*n.first)) return f;
        return find_active(*n.second);
    }
    static Node* find_parent(Node& n, Node* child) {
        if(n.is_leaf()) return nullptr;
        if(n.first.get() == child || n.second.get() == child) return &n;
        if(auto* p = find_parent(*n.first, child)) return p;
        return find_parent(*n.second, child);
    }
    static void clear_active(Node& n) {
        if(n.is_leaf()) n.active_here = false;
        else {
            clear_active(*n.first);
            clear_active(*n.second);
        }
    }
    static Node* leftmost_leaf(Node& n) {
        return n.is_leaf() ? &n : leftmost_leaf(*n.first);
    }

    void move_active(int dir) {
        std::vector<Node*> leaves;
        collect_leaves(m_root, leaves);
        if(leaves.size() < 2) return;

        auto   it = std::find_if(leaves.begin(), leaves.end(), [](Node* n) {
            return n->active_here;
        });
        size_t idx =
            (it == leaves.end()) ? 0 : static_cast<size_t>(it - leaves.begin());
        size_t next =
            (idx + leaves.size() + static_cast<size_t>(dir)) % leaves.size();

        for(auto* n : leaves) n->active_here = false;
        leaves[next]->active_here = true;
    }

    static void collect_leaves(Node& n, std::vector<Node*>& out) {
        if(n.is_leaf()) {
            out.push_back(&n);
            return;
        }
        collect_leaves(*n.first, out);
        collect_leaves(*n.second, out);
    }
};
}  // namespace trux::component
