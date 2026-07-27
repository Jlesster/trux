#include <chrono>
#include <thread>
#include <trux/terminal/terminal.hpp>

int main() {
    trux::Terminal terminal;

    auto result = terminal.init();

    if(!result) return 1;

    terminal.clear();

    std::this_thread::sleep_for(std::chrono::seconds(3));

    terminal.shutdown();
}
