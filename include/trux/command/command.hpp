#pragma once
#include <cstdint>

namespace trux::command {

using CommandID = uint32_t;

struct Command {
    CommandID id{};
    int       count = 1;
};

}  // namespace trux::command
