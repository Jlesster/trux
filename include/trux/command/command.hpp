/// @file command.hpp
/// @brief Command: a small, application-defined action identifier
///        paired with an optional repeat count (e.g. for vi-style
///        `3dd`-type bindings).

#pragma once
#include <cstdint>

namespace trux::command {

/// Opaque identifier for an application-defined command. Applications
/// typically define their own enum and `static_cast` its values to
/// this type.
using CommandID = uint32_t;

/// A command to execute, with an optional repeat count.
struct Command {
    /// Which command to run.
    CommandID id{};
    /// How many times to run it (e.g. the `3` in `3dd`). Defaults to 1.
    int       count = 1;
};

}  // namespace trux::command
