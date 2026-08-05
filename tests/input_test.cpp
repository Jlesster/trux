#include "test.hpp"
#include "trux/input/input.hpp"

#include <cassert>
#include <trux/input/event.hpp>
#include <trux/input/key.hpp>
using namespace trux;

void char_event_test() {
    input::Event event{.code = U'A', .valid = true};
    assert(event);
    assert(event.code == U'A');
}
void special_key_event_test() {
    input::Event event{.code = input::Key::Enter, .valid = true};
    assert(event);
    assert(event.code == input::Key::Enter);
}
void default_event_test() {
    input::Event event{};
    assert(!event);
}
void input_queue_test() {
    input::Input input;
    input.push('a');
    assert(input.available());
    auto event = input.poll();
    assert(event);
    assert(event.code == U'a');
    assert(!input.available());
}
int main() {
    test::run("character event", char_event_test);
    test::run("special key event", special_key_event_test);
    test::run("default event", default_event_test);
    test::run("input queue test", input_queue_test);
}
