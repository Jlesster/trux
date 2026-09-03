/// @file split.hpp
/// @brief Split: a runtime-resizable binary tree of components,
///        supporting interactive splitting, focus traversal between
///        panes, and closing panes (tmux/vim-window style).

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

/// A binary tree of components occupying a shared area, where exactly
/// one leaf is "active" at a time. Starts as a single leaf (the
/// `initial` component passed to the constructor); split_v()/split_h()
/// turn the active leaf into an internal node with two children,
/// growing the tree. focus_next()/focus_prev() (or the key predicates
/// set via set_focus_next_key()/set_focus_prev_key()) move which leaf
/// is active, and close_active() removes the active leaf, promoting
/// its sibling in its place.
///
/// Unlike Renderer's own split-region handling, Split owns its
/// children (type-erased via ComponentWrapper) and its tree shape can
/// change at runtime — it's meant for interactive layouts like a
/// tmux/vim-style pane manager.
struct Split {
    ComponentFlags flags{};

    /// A predicate over input events used to trigger focus_next()/
    /// focus_prev() from within handle() (see set_focus_next_key()).
    using KeyPredicate = std::function<bool(const input::Event&)>;

    /// Constructs a Split containing just `initial` as its one
    /// (active) leaf.
    template <ComponentType T> explicit Split(T initial) {
        m_root.leaf = std::make_unique<ComponentWrapper<T>>(std::move(initial));
        m_root.active_here = true;
    }

    /// Recursively lays out and builds every leaf in the tree within
    /// `area`, splitting the area at each internal node per its axis/ratio.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        build_node(m_root, area, cmd);
    }

    /// Sets the predicate checked first in handle(): when it matches
    /// an event, focus_next() runs instead of forwarding the event to
    /// the active leaf.
    void set_focus_next_key(KeyPredicate pred) {
        m_next_pred = std::move(pred);
    }
    /// @copydoc set_focus_next_key()
    /// Runs focus_prev() instead of focus_next() when matched.
    void set_focus_prev_key(KeyPredicate pred) {
        m_prev_pred = std::move(pred);
    }

    /// Checks the focus-navigation predicates first; if neither
    /// matches, forwards the event to the active leaf's handle().
    /// Returns whether the event was consumed.
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

    /// Moves the active leaf to the next one in tree order, wrapping
    /// to the first.
    void focus_next() { move_active(1); }
    /// Moves the active leaf to the previous one in tree order,
    /// wrapping to the last.
    void focus_prev() { move_active(-1); }

    /// Splits the active leaf vertically (side by side), placing
    /// `content` in the new second pane and making it active.
    template <ComponentType T> void split_v(T content) {
        split(layout::Axis::Vertical, std::move(content));
    }
    /// Splits the active leaf horizontally (stacked), placing
    /// `content` in the new second pane and making it active.
    template <ComponentType T> void split_h(T content) {
        split(layout::Axis::Horizontal, std::move(content));
    }

    /// Removes the active leaf and promotes its sibling (and the
    /// sibling's whole subtree) into its parent's place, then makes
    /// the promoted subtree's leftmost leaf active. A no-op if the
    /// active leaf is the tree's sole root (has no parent to promote into).
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
    /// One node in the split tree: either a leaf holding a
    /// type-erased component, or an internal node holding two child
    /// subtrees split along `axis` at `ratio`.
    struct Node {
        std::unique_ptr<ComponentBase> leaf;
        layout::Axis                   axis{};
        /// Fraction of the area given to `first` (0.0-1.0).
        float                          ratio{0.5f};
        std::unique_ptr<Node>          first, second;
        /// Whether this leaf is the currently active one. Only
        /// meaningful (and exclusive across the tree) for leaves.
        bool                           active_here{false};

        /// Whether this node is a leaf (holds a component directly)
        /// rather than an internal split node.
        [[nodiscard]]
        bool is_leaf() const noexcept {
            return leaf != nullptr;
        }
    };

    Node         m_root;
    KeyPredicate m_next_pred;
    KeyPredicate m_prev_pred;

    /// Turns the active leaf into an internal node split along
    /// `axis`, keeping its existing component in the first child and
    /// placing `content` in a new second child, which becomes active.
    /// A no-op if there is no active leaf (shouldn't normally happen).
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

    /// Recursively builds `n`: if it's a leaf, builds its component
    /// directly into `area`; otherwise splits `area` per `n.axis`/
    /// `n.ratio` and recurses into both children.
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

    /// Finds the currently active leaf beneath `n`, or nullptr if
    /// none is marked active (shouldn't normally happen for a
    /// well-formed tree).
    static Node* find_active(Node& n) {
        if(n.is_leaf()) return n.active_here ? &n : nullptr;
        if(auto* f = find_active(*n.first)) return f;
        return find_active(*n.second);
    }
    /// Finds the internal node beneath `n` that directly parents `child`.
    static Node* find_parent(Node& n, Node* child) {
        if(n.is_leaf()) return nullptr;
        if(n.first.get() == child || n.second.get() == child) return &n;
        if(auto* p = find_parent(*n.first, child)) return p;
        return find_parent(*n.second, child);
    }
    /// Clears active_here on every leaf beneath `n`.
    static void clear_active(Node& n) {
        if(n.is_leaf()) n.active_here = false;
        else {
            clear_active(*n.first);
            clear_active(*n.second);
        }
    }
    /// The leftmost (first-child-following) leaf beneath `n`.
    static Node* leftmost_leaf(Node& n) {
        return n.is_leaf() ? &n : leftmost_leaf(*n.first);
    }

    /// Moves the active leaf `dir` steps (1 or -1) through the
    /// tree's leaves in traversal order, wrapping around. A no-op if
    /// fewer than two leaves exist.
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

    /// Appends every leaf beneath `n`, in tree (left-to-right) order, to `out`.
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
