#include "test.hpp"
#include "trux/input/input.hpp"

#include <cassert>
#include <trux/input/event.hpp>
#include <trux/input/key.hpp>

using namespace trux;

void char_event_test() {
    input::Event event{
        input::KeyEvent{.key = input::Key::Char, .character = U'A'}
    };

    auto& key = std::get<input::KeyEvent>(event.data);

    assert(key.key == input::Key::Char);
    assert(key.character == U'A');
}

void special_key_event_test() {
    input::Event event{input::KeyEvent{.key = input::Key::Enter}};

    auto& key = std::get<input::KeyEvent>(event.data);

    assert(key.key == input::Key::Enter);
}

void default_event_test() {
    input::Event event{};

    auto& key = std::get<input::KeyEvent>(event.data);

    assert(key.key == input::Key::Unknown);
}

void input_queue_test() {
    input::Input input;

    input.push('a');
    assert(input.available());

    auto event = input.poll();
    assert(event.has_value());

    auto key = std::get<input::KeyEvent>(event->data);

    assert(key.key == input::Key::Char);
    assert(key.character == U'a');
    assert(!input.available());
}

int main() {
    test::run("character event", char_event_test);
    test::run("special key event", special_key_event_test);
    test::run("default event", default_event_test);
    test::run("input queue test", input_queue_test);
}
