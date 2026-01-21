# ⚡ Mach

**Ultra-fast HTTP load tester written in C with hand-optimized Assembly.**

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Size](https://img.shields.io/badge/binary-52KB-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)

## Features

- 🚀 **Extreme Performance** – Raw POSIX sockets + pthreads
- 🔧 **Hand-Written ASM** – ARM64 NEON & x86_64 SSE optimized
- 📦 **Tiny Binary** – ~52 KB stripped
- 🛡️ **HTTPS Support** – OpenSSL 3 integration
- 📊 **Rich Stats** – P50, P95, P99 latencies + status codes
- 🖥️ **Interactive Dashboard** – Navigate test history with arrow keys

## Install

**One-liner (Linux/macOS):**
```bash
curl -fsSL https://raw.githubusercontent.com/HiteshGorana/mach/main/install.sh | sh
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/HiteshGorana/mach/main/install.ps1 | iex
```

**Or download from [Releases](https://github.com/HiteshGorana/mach/releases)**

## Quick Start

```bash
# Build
make

# Run a quick load test
./mach http://localhost:8080

# Custom load
./mach -n 1000 -c 50 http://example.com

# Duration-based soak test
./mach -d 5m -c 20 http://api.example.com
```

## Usage

```
⚡ Mach

Usage: mach [command] [options] <url>

Commands:
  attack      Full-featured load test
  dashboard   View historical test runs
  history     history clear, history list
  examples    Show usage examples
  version     Show version information

Options:
  -n INT      Total requests (default 100)
  -d STR      Run duration (e.g., 30s, 1m, 5m)
  -c INT      Concurrent workers (default 10)
  -r INT      Requests per second limit
  -p STR      Test profile (smoke, stress, soak)
  -m STR      HTTP method (default GET)
```

## Profiles

| Profile | Requests | Concurrency | Description |
| :--- | :--- | :--- | :--- |
| `smoke` | 10 | 2 | Quick sanity check |
| `stress` | 10,000 | 100 | High load test |
| `soak` | (duration) | 50 | Long-running stability test |

## Cross-Platform

The Makefile auto-detects your platform and architecture:

```bash
# macOS (Apple Silicon)
Built for Darwin arm64

# Linux (x86_64)
Built for Linux x86_64

# Windows (MinGW)
Built for Windows
```

## ASM Optimization

Critical hot paths use hand-written Assembly:

| Function | ARM64 | x86_64 |
| :--- | :--- | :--- |
| `fast_sum` | NEON `fadd` | SSE2 `addpd` |
| `fast_min` | NEON `fmin` | SSE2 `minpd` |
| `fast_max` | NEON `fmax` | SSE2 `maxpd` |
| `fast_parse_status` | Optimized loop | Optimized loop |
| `fast_duration_ms` | FP math | FP math |

## License

MIT
