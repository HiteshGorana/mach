#include "blitz.h"
#include <termios.h>

static struct termios orig_termios;

void terminal_make_raw() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void terminal_restore() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

int terminal_read_key() {
  char c;
  if (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\033') {
      char seq[3];
      if (read(STDIN_FILENO, &seq[0], 1) != 1)
        return '\033';
      if (read(STDIN_FILENO, &seq[1], 1) != 1)
        return '\033';

      if (seq[0] == '[') {
        switch (seq[1]) {
        case 'A':
          return 1000; // UP
        case 'B':
          return 1001; // DOWN
        case 'C':
          return 1002; // RIGHT
        case 'D':
          return 1003; // LEFT
        }
      }
    }
    return c;
  }
  return 0;
}
