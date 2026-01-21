# ⚡ Mach

**Ultra-fast HTTP load tester written in C with hand-optimized Assembly.**

[![Build](https://github.com/HiteshGorana/mach/actions/workflows/build.yml/badge.svg)](https://github.com/HiteshGorana/mach/actions/workflows/build.yml)
![Size](https://img.shields.io/badge/binary-52KB-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

---

## ✨ Features

| Feature | Description |
|:--------|:------------|
| 🚀 **Extreme Performance** | Raw POSIX sockets + pthreads |
| 🔧 **Hand-Written ASM** | ARM64 NEON & x86_64 SSE optimized |
| 📦 **Tiny Binary** | ~52 KB stripped |
| 🛡️ **HTTPS Support** | OpenSSL 3 integration |
| 📊 **Rich Stats** | P50, P95, P99 latencies + status codes |
| 🖥️ **Interactive Dashboard** | Navigate test history with arrow keys |
| 📋 **Test Profiles** | Smoke, stress, and soak presets |
| 📉 **Regression Testing** | Before/After comparison with tags |
| 🛡️ **Perf Gates** | Build-breaking regression thresholds |

---

## 📥 Install

### One-liner (Linux/macOS)
```bash
curl -fsSL https://raw.githubusercontent.com/HiteshGorana/mach/main/install.sh | sh
```

### Windows (PowerShell)
```powershell
irm https://raw.githubusercontent.com/HiteshGorana/mach/main/install.ps1 | iex
```

### Manual Download
Download pre-built binaries from [**Releases**](https://github.com/HiteshGorana/mach/releases)

---

## 🚀 Quick Start

```bash
# Simple test
mach http://localhost:8080

# 1000 requests with 50 concurrent workers
mach -n 1000 -c 50 http://example.com

# 5-minute soak test
mach -d 5m -c 20 http://api.example.com

# Stress test profile
mach --profile stress http://example.com
```

---

## 💡 Recipes

### JSON API Test (POST)
```bash
mach -m POST \
     -h "Content-Type: application/json" \
     -b '{"name": "test", "value": 123}' \
     https://api.example.com/v1/data
```

### Auth Protected Endpoint
```bash
mach -h "Authorization: Bearer YOUR_TOKEN" \
     https://api.example.com/secure
```

### High-Concurrency Stress Test
```bash
# 500 workers, 10k requests, skip TLS verification
mach -n 10000 -c 500 --insecure https://stage.api.com
```

### Throttled API Test (Rate Limiting)
```bash
# Limit to 50 requests per second
mach -r 50 -d 30s https://api.example.com
```

### Regression Testing (Before/After)
Track performance improvements or regressions across code changes.

```bash
# 1. Capture baseline (before optimization)
mach --tag perf-fix --before -n 1000 http://api.com

# 2. Capture target (after optimization)
mach --tag perf-fix --after -n 1000 http://api.com

# 3. View comparison table
mach --tag perf-fix --result
```

### CI Performance Gate
Automatically fail CI/CD pipelines if performance regresses beyond a threshold.

```bash
# Fail if avg latency regresses by more than 5%
mach --tag release-1.1 --after --threshold 5 -n 1000 http://api.com
```

### Testing Multiple URLs
Create a `urls.txt`:
```
https://example.com/page1
https://example.com/page2
https://example.com/api/v1
```
Run:
```bash
mach -n 1000 -c 20 urls.txt
```

---

## 📖 Usage

```
⚡ Mach

Usage: mach [command] [options] <url>

Commands:
  attack      Full-featured load test
  dashboard   View historical test runs
  history     Manage test history (clear, list)
  examples    Show usage examples
  version     Show version information

Options:
  -n INT      Total requests (default 100)
  -d STR      Run duration (e.g., 30s, 1m, 5m)
  -c INT      Concurrent workers (default 10)
  -r INT      Requests per second limit
  -p STR      Test profile (smoke, stress, soak)
  -m STR      HTTP method (default GET)
  -h STR      Add header (e.g., "Authorization:Bearer token")
  -b STR      Request body
  --insecure  Skip TLS verification
  --tag STR   Tag name for comparison
  --before    Set as baseline for tag
  --after     Set as target for tag comparison
  --result    Show comparison result for tag
  --threshold INT Max allowed regression %
```

---

## 🎯 Profiles

| Profile | Requests | Concurrency | Use Case |
|:--------|:---------|:------------|:---------|
| `smoke` | 10 | 2 | Quick sanity check |
| `stress` | 10,000 | 100 | High load testing |
| `soak` | Duration | 50 | Long-running stability |

---

## 🔧 Build from Source

```bash
# Clone
git clone https://github.com/HiteshGorana/mach.git
cd mach

# Build (auto-detects platform)
make

# Run
./mach http://example.com
```

**Requirements:**
- GCC or Clang
- OpenSSL 3 (`brew install openssl@3` on macOS)
- Make

---

## ⚡ ASM Optimization

Critical hot paths use hand-written Assembly for maximum performance:

| Function | ARM64 (Apple Silicon) | x86_64 (Intel/AMD) |
|:---------|:---------------------|:-------------------|
| `fast_sum` | NEON `fadd` | SSE2 `addpd` |
| `fast_min` | NEON `fmin` | SSE2 `minpd` |
| `fast_max` | NEON `fmax` | SSE2 `maxpd` |
| `fast_parse_status` | Optimized loop | Optimized loop |
| `fast_duration_ms` | FP math | FP math |

---

## 🌍 Cross-Platform

Mach runs natively on:
- ✅ **macOS** (ARM64 Apple Silicon & Intel x86_64)
- ✅ **Linux** (x86_64)
- ✅ **Windows** (x86_64 via MinGW)

---

## 📄 License

MIT © [Hitesh Gorana](https://github.com/HiteshGorana)
