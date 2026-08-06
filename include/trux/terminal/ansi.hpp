#pragma once

namespace trux::ansi {

void hide_cursor();
void show_cursor();

void enter_alt_screen();
void leave_alt_screen();

void enable_kitty_keyboard();
void disable_kitty_keyboard();

void enable_mouse();
void disable_mouse();

void enable_bracketed_paste();
void disable_bracketed_paste();

void clear();
void flush();
}  // namespace trux::ansi
