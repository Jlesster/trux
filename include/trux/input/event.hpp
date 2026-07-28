#pragma once

#include <trux/input/key.hpp>
#include <variant>

namespace trux::input {

using EventData = std::variant<trux::input::KeyEvent>;

struct Event {
    EventData data;
};
}  // namespace trux::input
