# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
make                 # build ./mach for the host platform (Darwin/Linux/MinGW auto-detected)
make clean           # remove obj/, mach, mach.exe
make debug           # -O0 -g3 -DDEBUG, unstripped
make asan            # ASan + UBSan build; use for memory-sensitive C changes
./mach http://example.com                  # smoke test
./mach --profile smoke http://example.com  # profile smoke test
```

- There is no automated test suite or linter. Verification is building plus CLI smoke runs (see `AGENTS.md`).
- OpenSSL 3 is required. On macOS the Makefile hardcodes `/opt/homebrew/opt/openssl@3` for include/lib paths.
- CI (`.github/workflows/build.yml`) builds Linux x86_64, macOS x86_64 and arm64, and Windows (MSYS2/MinGW64). A `v*` tag triggers a release.
- Performance changes should be compared with `--tag NAME --before` / `--after` / `--result` (writes to `~/.mach/tags/`).

## Architecture

**Single binary, pipeline of modules.** `main.c` parses args and dispatches subcommands (`attack`, `dashboard`/`dash`, `history [clear]`, `examples`, `update`, `version`); a bare URL or `-n/-c/...` flags fall through to an attack run. Options are collected into one `Options` struct declared in `blitz.h`, which is shared by every module. `blitz.h` is also the common include (platform headers, pthread, OpenSSL) and holds `VERSION`, the `Header`/`Result` types and the limits `MAX_URLS`/`MAX_HEADERS`.

**Load generation (`attacker.c`).** `attacker_run` spawns `opts->concurrency` pthreads, each running `worker_thread` with a `WorkerContext`. Each worker owns its own connection(s) from `http.c` and writes `Result` records into a shared array under one mutex (`record_result`). Request count is split across workers, a worker's start is staggered for ramp-up, and a global RPS limit is enforced with a shared throttle (`throttle_global_rps`). Duration-based runs and the stop flag are coordinated through the same mutex-guarded state. The main thread polls progress with `usleep`.

**HTTP layer (`http.c` on POSIX, `http_win.c` on Windows).** Raw sockets with OpenSSL for HTTPS, same interface in `http.h`. `url.c` parses the supported URL forms; `http_send` returns a `Result` with status, duration and error. The platform file is chosen in the Makefile, so platform differences should stay inside these files and their `terminal*.c` counterparts.

**Statistics (`stats.c` + `src/asm/`).** `calculate_stats` turns the raw `Result` array into `Stats` (avg/min/max, p50/p95/p99, status-code histogram, RPS). The hot numeric kernels `fast_sum`, `fast_min`, `fast_max` (and `fast_parse_status` in the x86 file) are assembly, linked in via `extern` declarations. Each architecture has its own file (`fast_stats_arm64.s` for Apple Silicon, `fast_stats_x86.s` for everything else); the Makefile picks one. Any change to these functions must be made in both files or the non-matching platform breaks. The assembly exports both `_name` (Mach-O) and `name` (ELF/COFF) symbols.

**Persistence (`storage.c`).** Everything lives under `$HOME/.mach/` (or the user profile on Windows): `history/*.json` for each run and `tags/<tag>/{before,after}.json` for regression comparison. `--threshold` in a tagged `--after` run is what makes CI fail on regression. `ui.c` reads these files for the dashboard and `--result` table.

**UI (`ui.c`, `terminal.c` / `terminal_win.c`).** Rendering, colors, and the interactive dashboard that navigates history with arrow keys. Terminal input handling is split between `terminal.c` and `terminal_win.c`.

**Self-update (`updater.c`).** Queries the GitHub releases API for `HiteshGorana/mach` (`http_fetch_body` / `http_download_to_file`). Reached via `mach update`.

**Install scripts** (`scripts/install.sh`, `scripts/install.ps1`) are the user-facing install path; they are separate from the Makefile.

## Conventions

Follow `AGENTS.md` for style (two-space C indentation, `snake_case`, Conventional Commit subjects). Keep platform code in the POSIX or Windows files rather than adding `#ifdef` blocks to core modules.
