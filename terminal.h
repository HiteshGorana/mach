#ifndef TERMINAL_H
#define TERMINAL_H

void terminal_make_raw();
void terminal_restore();
int terminal_read_key();

#endif
