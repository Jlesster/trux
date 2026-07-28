#include "test.hpp"
#include "trux/input/key.hpp"

#include <cassert>
#include <trux/input/event_parser.hpp>

using namespace trux;

void printable_char_test() {
    input::EventParser parser;

    auto event = parser.parse('A');
    assert(event.has_value());

    auto key = std::get<input::KeyEvent>(event->data);
    assert(key.key == input::Key::Char);
    assert(key.character == U'A');
}

void null_char_test() {
    input::EventParser parser;

    auto event = parser.parse('\n');
    assert(!event.has_value());
}

void enter_test() {
    input::EventParser parser;

    auto event = parser.parse('\n');
    assert(event);

    auto key = std::get<input::KeyEvent>(event->data);
    assert(key.key == input::Key::Enter);
}

void arrow_up_test() {
    input::EventParser parser;

    assert(!parser.parse('\x1b'));
    assert(!parser.parse('['));

    auto event = parser.parse('A');
    assert(event);

    auto key = std::get<input::KeyEvent>(event->data);
    assert(key.key == input::Key::Up);
}

int main() {
    test::run("printable char test", printable_char_test);
    test::run("null char test", null_char_test);
    test::run("enter test", enter_test);
    test::run("up arrow test", arrow_up_test);
}
