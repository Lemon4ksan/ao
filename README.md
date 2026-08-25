<div align="center">

# ao

### The Stealth Silicon HTTP Client powered by `libaoni`

[![C Standard](https://img.shields.io/badge/c-c11%20%2F%20c99-00599C?logo=c&style=flat-square)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Engine](https://img.shields.io/badge/engine-libaoni-blueviolet?style=flat-square)](#architecture--silicon-bridge)
[![uTLS Evasion](https://img.shields.io/badge/uTLS-JA4%20%2F%20p0f%20Spoofing-success?style=flat-square)](#1-evasion-first-browser-impersonation--network-fingerprinting)
[![Post-Quantum](https://img.shields.io/badge/PQ-ML--KEM--768-orange?style=flat-square)](#2-post-quantum-cryptography-ml-kem-768)
[![Zero Alloc](https://img.shields.io/badge/memory-0%25%20GC%20Off--Heap-brightgreen?style=flat-square)](#4-silicon-core--0-alloc-off-heap-reactor)
[![License](https://img.shields.io/badge/license-curl%20%2F%20BSD--like-informational?style=flat-square)](COPYING)

> _"The familiar ergonomics of curl, backed by the stealth and raw throughput of the silicon reactor."_

</div>

## Overview

**`ao`** is an independent, high-performance HTTP client fork of `curl` engineered on top of the silicon transport engine **`libaoni`**. It unites the universal command-line ergonomics and scripting compatibility of `curl` with native browser stealth evasion (Chromium/Firefox uTLS, JA4, RFC 9460 Encrypted ClientHello, TCP p0f spoofing), hybrid Post-Quantum cryptography, and a zero-allocation Off-Heap memory architecture.

While standard `curl` relies on standard OpenSSL/GnuTLS TLS stacks - exposing static, easily fingerprinted handshakes that are flagged by modern Web Application Firewalls (Cloudflare Bot Management, Akamai Edge, Datadome, AWS CloudFront) - `ao` routes traffic through the `libaoni` reactor to emit byte-for-byte identical signatures to real browser sessions.

```bash
# Execute an evasive request disguised as modern Chromium with Post-Quantum key exchange
ao --aoni-browser=chrome https://tls.browserleaks.com/json
```

## Key Capabilities

### 1. Evasion-First (Browser Impersonation & Network Fingerprinting)
* **Dynamic ClientHello Emulation**: Emulates the exact TLS handshake signatures of modern desktop and mobile browsers (Chrome, Firefox, Safari, iOS, Android), including cipher suite order, TLS extension permutation, GREASE insertion, supported groups, and ALPN tokens.
* **JA4 / JA3 Fingerprint Control**: Generates cryptographically authentic JA4 and JA3 signatures to prevent heuristics-based bot scoring.
* **TCP / p0f Stack Spoofing**: Synchronizes L4 network packet characteristics (IP TTL, TCP Window Size, MSS, SACK permissions, TCP Options order) with the target browser operating system.
* **Encrypted Client Hello (ECH RFC 9460)**: Full native ECH support via DoH/DoQ DNS resolution to eliminate SNI leaks across perimeter monitoring systems.
* **Anti-Bot Resistance**: Built to navigate Cloudflare Turnstile, Akamai Edge, Datadome, and perimeter gatekeepers without headless browser memory overhead.

### 2. Post-Quantum Cryptography (ML-KEM-768)
* **Hybrid Quantum-Resistant Key Exchange**: Native support for standardized Post-Quantum key encapsulation mechanisms (**ML-KEM-768** / **X25519Kyber768**).
* **Forward Secrecy**: Protects encrypted communications against future *Harvest Now, Decrypt Later* adversary decryption models.

### 3. Full-Duplex Streaming & Modern Protocols
* **Native HTTP/3 & QUIC Core**: Native multiplexed HTTP/3 transport powered by `libaoni` with 0-RTT connection establishment.
* **Terminal WebSocket & SSE**: Interactive, line-buffered terminal WebSocket (`ws://`, `wss://`) and Server-Sent Events (SSE) streaming with low-latency frame dispatch.

### 4. Silicon Core & 0-Alloc Off-Heap Reactor
* **0% GC Off-Heap Memory Model**: Completely bypasses managed memory runtimes, operating within pre-allocated off-heap memory arenas.
* **Zero-Copy Ring Buffers**: High-throughput packet reactor optimized for multi-gigabit link saturation and sub-microsecond packet scheduling.

## Technical Comparison

| Capability | Standard `curl` | `ao` (`libaoni`) |
| :--- | :---: | :---: |
| **CLI Syntax & Option Parity** | 100% | **100% Compatible** |
| **uTLS Browser Fingerprint Emulation** | None (OpenSSL defaults) | **Chrome, Firefox, Safari, Mobile** |
| **JA4 / JA3 Fingerprint Matching** | Static / Detectable | **Bit-exact emulation + GREASE** |
| **TCP / p0f Network Stack Spoofing** | OS Kernel dependent | **Configurable SYN/TCP options** |
| **Encrypted Client Hello (ECH RFC 9460)** | Experimental / External | **Native Silicon Reactor** |
| **Post-Quantum Key Exchange (ML-KEM)** | Provider dependent | **Native ML-KEM-768 / X25519Kyber768** |
| **Multi-Threaded RPS (100 threads)** | ~1,500 - 3,000 RPS (Lock contention) | **9,145+ RPS (100% Success, Zero GC)** |
| **Memory Management** | Standard `malloc` | **Zero-Alloc Off-Heap Ring Buffers** |
| **Terminal WebSocket & SSE** | Basic | **Full-Duplex Interactive Engine** |
| **WAF / Anti-Bot Bypass** | Flagged by default | **Evasion-First Architecture** |

## Performance & Multi-Threaded Benchmarks

`ao` completely eliminates standard curl's multi-threading bottlenecks (SSL session lock contention, synchronous DNS resolver locks, and heap fragmentation) by delegating network operations to `libaoni`'s non-blocking reactor and off-heap memory arenas.

### Stress Test: 100 Concurrent POSIX Threads Calling `curl_easy_perform`

```text
====================================================
  ao (curl on libaoni) 100-Thread Stress Benchmark  
====================================================
Target URL   : http://127.0.0.1:8888/bench
Concurrency  : 100 threads
Requests     : 50,000 total (500/thread)

=== Results ===
Total Requests : 50,000
Successful     : 50,000 (100.00% Success Rate)
Failed         : 0 (0.00% Dropped / Socket Errors)
Elapsed Time   : 5.467 s
Throughput     : 9,145.13 RPS
Avg Req Time   : 10.935 ms
====================================================
```

| Concurrency | Total Requests | Elapsed Time | Throughput | Success Rate | Memory Model |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **50 threads** | 20,000 | 2.274 s | **8,794.75 RPS** | **100.00%** | 0% GC / Off-Heap |
| **100 threads** | 50,000 | 5.467 s | **9,145.13 RPS** | **100.00%** | 0% GC / Off-Heap |

## Architecture & Silicon Bridge

The integration between `curl`'s core interface and `libaoni` is executed through the zero-overhead C bridge (`lib/aoni_bridge.c`, `lib/aoni_bridge.h`):

```
+-------------------------------------------------------------+
|                          ao CLI                             |
|      (Command-line parser, argument validation, POSIX UX)   |
+------------------------------+------------------------------+
                               |
                   libcurl internal transfer
                               |
+------------------------------v------------------------------+
|                        aoni_bridge                          |
|         (lib/aoni_bridge.c  /  lib/aoni_bridge.h)           |
+------------------------------+------------------------------+
                               |
                   Off-Heap C / FFI Dispatch
                               |
+------------------------------v------------------------------+
|                          libaoni                            |
|  * uTLS Dynamic Evasion Engine (Chrome / Firefox / Safari)  |
|  * Post-Quantum Hybrid Cryptography (ML-KEM-768)            |
|  * JA4 / p0f Network Stack & TCP Options Generator         |
|  * RFC 9460 Encrypted Client Hello (ECH via DoH/DoQ)        |
|  * HTTP/3 / QUIC Protocol State Machine                     |
|  * Zero-Copy Ring Buffer & Off-Heap Memory Arenas           |
+-------------------------------------------------------------+
```

## Usage & CLI Reference

### 1. Browser Profile Emulation
```bash
# Impersonate modern Chromium with default uTLS signature and HTTP/2 settings
ao --aoni-browser=chrome https://api.example.com/endpoint

# Impersonate Firefox with custom headers
ao --aoni-browser=firefox -H "Accept-Language: en-US,en;q=0.9" https://api.example.com/feed
```

### 2. Post-Quantum Cryptography
```bash
# Force hybrid ML-KEM-768 post-quantum key exchange
ao --aoni-browser=chrome --aoni-pq=mlkem768 https://pq.example.com
```

### 3. Full-Duplex Terminal WebSocket
```bash
# Connect to an interactive WebSocket stream
ao --websocket wss://stream.example.com/socket
```

### 4. Standard Curl Options Parity
All standard `curl` flags (`-X`, `-H`, `-d`, `-o`, `-v`, `-s`, `-L`, `--compressed`, etc.) operate identically:
```bash
ao -X POST https://api.target.com/v1/telemetry \
   --aoni-browser=chrome \
   -H "Content-Type: application/json" \
   -d '{"event":"handshake","timestamp":1724600000}'
```

## Building from Source

### Prerequisites
* **Go Compiler** `>= 1.22` (for compiling `libaoni`)
* **CMake** `>= 3.16`
* **C Compiler** (GCC `>= 8`, Clang `>= 10`, or MSVC 2019+)

### Step 1: Build `libaoni` Engine

`ao` requires the compiled `libaoni` shared library and headers.

```bash
# Clone and build libaoni
git clone https://github.com/lemon4ksan/aoni
cd aoni

# Compile libaoni.so (Linux/macOS) or aoni.dll (Windows)
make lib
```

This generates:
- `aoni/bin/libaoni.so` (or `aoni.dll` / `libaoni.dylib`)
- `aoni/include/aoni.h` (C99 ABI interface)

### Step 2: Build `ao` (`curl` powered by `libaoni`)

Place `ao` adjacent to `aoni` (or set `-DAONI_LIB=/path/to/libaoni.so`):

```bash
# Clone ao repository
git clone https://github.com/Lemon4ksan/ao
cd ao

# Configure build with CMake (auto-detects ../aoni/bin/libaoni.so)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_CURL_EXE=ON

# Compile the ao binary
cmake --build build --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
```

The compiled binary will be located at:
- `build/src/ao` (Linux / macOS)
- `build/src/Release/ao.exe` (Windows)

### Verifying the Build

Run a stealth TLS test to verify that `ao` is executing through `libaoni`:

```bash
./build/src/ao -s https://tls.peet.ws/api/all | grep -E "ja4|http_version"
# Output should show:
# "http_version": "h2",
# "ja4": "t13d1515h2_8daaf6152771_22334254f9f7"
```

## Repository Layout

```
ao/
├── include/          // Public curl and client API headers (curl/curl.h)
├── lib/              // Core engine implementation
│   ├── aoni_bridge.c // Bridge implementation connecting libcurl to libaoni
│   ├── aoni_bridge.h // Bridge definitions and configuration contracts
│   └── ...           // Protocol handlers, transfer state machines
├── src/              // CLI client frontend (command-line parsing, terminal UX)
├── CMake/            // CMake toolchains and feature detection modules
├── docs/             // Technical specifications and protocol documentation
├── scripts/          // Code generation and verification scripts
└── tests/            // Integration and compliance test suite
```

## License

`ao` is distributed under the original curl / BSD-derived license. See [COPYING](COPYING) and [LICENSES/](LICENSES/) for terms and conditions.

<div align="center">
  <sub>Engineered for uncompromising evasion and silicon-grade performance.</sub>
</div>
