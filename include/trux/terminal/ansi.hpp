#pragma once

namespace trux::ansi {

void hide_cursor();
void show_cursor();

void enter_alt_screen();
void leave_alt_screen();

void clear();
void flush();
}  // namespace trux::ansi
