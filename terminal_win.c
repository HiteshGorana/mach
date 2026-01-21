// terminal_win.c - Windows Console API implementation
#ifdef _WIN32

#include "terminal.h"
#include <conio.h>
#include <windows.h>

static HANDLE hConsole;
static DWORD originalMode;

void terminal_make_raw() {
  hConsole = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hConsole, &originalMode);
  SetConsoleMode(hConsole, ENABLE_EXTENDED_FLAGS);
}

void terminal_restore() { SetConsoleMode(hConsole, originalMode); }

int terminal_read_key() {
  int ch = _getch();

  // Handle arrow keys (0xE0 prefix on Windows)
  if (ch == 0 || ch == 0xE0) {
    ch = _getch();
    switch (ch) {
    case 72:
      return 1000; // UP
    case 80:
      return 1001; // DOWN
    case 75:
      return 1002; // LEFT
    case 77:
      return 1003; // RIGHT
    }
  }

  return ch;
}

#endif // _WIN32
