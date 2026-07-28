#include <unistd.h>

#include <optional>
#include <trux/terminal/terminal.hpp>

using namespace trux;

std::optional<char> Terminal::read() {
    char byte{};

    auto result = ::read(STDIN_FILENO, &byte, 1);

    if(result <= 0) return std::nullopt;

    return byte;
}
