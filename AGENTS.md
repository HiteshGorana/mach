# Repository Guidelines

## Project Structure & Module Organization

Mach is a cross-platform C command-line HTTP load tester. Core source lives in `src/`: `main.c` handles CLI parsing, `attacker.c` runs load tests, `stats.c` computes metrics, `storage.c` manages history, `ui.c` renders terminal output, `url.c` parses supported URLs, and `updater.c` handles self-update behavior. Shared interfaces are in matching `.h` files. Platform-specific implementations live beside the core code as `http.c`/`terminal.c` for POSIX systems and `http_win.c`/`terminal_win.c` for Windows. Hand-optimized assembly is under `src/asm/`. Install scripts are in `scripts/`. Release automation lives under `.github/workflows/`.

## Build, Test, and Development Commands

- `make` builds the native `mach` binary for the current platform.
- `make clean` removes `obj/`, `mach`, and `mach.exe`.
- `make debug` builds with symbols and no stripping.
- `make asan` builds with address and undefined-behavior sanitizers.
- `./mach http://example.com` runs a basic smoke test after building.
- `./mach --profile smoke http://example.com` verifies profile handling with a short run.

OpenSSL 3 is required for native builds. On macOS, install it with `brew install openssl@3`; the `Makefile` expects Homebrew paths under `/opt/homebrew/opt/openssl@3`.

## Coding Style & Naming Conventions

Use two-space indentation in C files, braces on the same line, and `snake_case` for functions, variables, and struct fields. Keep public declarations in headers and implementation details `static` where possible. Add platform-specific behavior to the existing POSIX or Windows files instead of scattering `#ifdef` blocks through core logic. Keep assembly filenames aligned with architecture, such as `fast_stats_arm64.s` and `fast_stats_x86.s`.

## Testing Guidelines

There is no dedicated automated test suite yet. Before opening a PR, run `make clean`, `make`, and targeted CLI smoke checks for a normal URL, a profile run, and any touched option. Use `make asan` for memory-sensitive C changes. For performance-sensitive changes, capture before/after runs with `--tag`, `--before`, `--after`, and `--result`.

## Commit & Pull Request Guidelines

Recent history uses Conventional Commit prefixes such as `feat:`, `fix:`, `docs:`, `refactor:`, and `chore:`. Keep commit subjects imperative and scoped to one change. Pull requests should describe behavior changes, list manual validation commands, note affected platforms, and include screenshots or terminal output when changing CLI behavior.

## Security & Configuration Tips

Do not commit build outputs, local history directories such as `.mach/`, dependency folders, or secrets. Keep local endpoints, tokens, and test credentials in environment-specific files that stay ignored.
