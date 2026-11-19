#pragma once
#include <concepts>
#include <cstddef>
#include <functional>
#include <string_view>
#include <type_traits>

#if defined(TRUX_PLAIN_ERRORS)
#define TRUX_ERR_RED ""
#define TRUX_ERR_BOLD ""
#define TRUX_ERR_BLUE ""
#define TRUX_ERR_CYAN ""
#define TRUX_ERR_RESET ""
#else
#define TRUX_ERR_RED "\x1b[1;31m"
#define TRUX_ERR_BOLD "\x1b[1m"
#define TRUX_ERR_BLUE "\x1b[1;34m"
#define TRUX_ERR_CYAN "\x1b[1;36m"
#define TRUX_ERR_RESET "\x1b[0m"
#endif

/// Line break + indent that lines text up under "= why:  " / "= help: ".
#define TRUX_NL "\n          "

#define TRUX_ERROR_MESSAGE(code, title, why, help)                          \
    "\n\n" TRUX_ERR_RED "error[" code "]" TRUX_ERR_RESET TRUX_ERR_BOLD      \
    ": " title TRUX_ERR_RESET "\n   " TRUX_ERR_BLUE "= why:" TRUX_ERR_RESET \
    "  " why "\n   " TRUX_ERR_CYAN "= help:" TRUX_ERR_RESET " " help "\n"

/// static_assert in trux's layout. The condition goes LAST (variadic, so
/// `f<A, B>` is fine) and must be a plain bool, not a concept.
#define TRUX_CHECK(code, title, why, help, ...) \
    static_assert(                              \
        __VA_ARGS__,                            \
        std::string_view{TRUX_ERROR_MESSAGE(code, title, why, help)})

namespace trux::diag::check {

template <typename...>
inline constexpr bool never = false;

template <typename R>
inline constexpr bool has_size = requires(R& r) {
    { r.size() } -> std::convertible_to<std::size_t>;
};

template <typename R>
inline constexpr bool has_index = requires(R& r, std::size_t i) { r[i]; };

template <typename R, typename Proj>
inline constexpr bool projects_to_text = requires(R& r, Proj p, std::size_t i) {
    { std::invoke(p, r[i]) } -> std::convertible_to<std::string_view>;
};

}  // namespace trux::diag::check
