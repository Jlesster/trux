#pragma once

#include "trux/layout/size.hpp"
#include "trux/renderer/renderer.hpp"

#include <expected>
#include <memory>
#include <optional>

namespace trux::input {
struct Event;
}

namespace trux {

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&)            = delete;
    Terminal& operator=(const Terminal&) = delete;

    Terminal(Terminal&&) noexcept;
    Terminal& operator=(Terminal&&) noexcept;

    [[nodiscard]]
    std::expected<void, std::string> init();
    std::expected<void, std::string> enable_raw_mode();

    [[nodiscard]]
    bool should_quit() const noexcept;
    void shutdown();

    [[nodiscard]]
    layout::Size size() const;

    void present(renderer::Renderer&);

    [[nodiscard]]
    bool dispatch(const input::Event&) const;
    [[nodiscard]]
    std::optional<char> read();
    [[nodiscard]]
    bool has_pending(int timeout_ms = 0) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    renderer::Renderer*   m_renderer = nullptr;
};
}  // namespace trux
