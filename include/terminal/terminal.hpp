#pragma once

#include <expected>
#include <memory>
#include <trux/size.hpp>

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

    void shutdown();

    [[nodiscard]]
    Size size() const;

    void clear();
    void present();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}  // namespace trux
