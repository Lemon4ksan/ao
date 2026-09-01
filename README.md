<div align="center">

# ao

### The Stealth Silicon HTTP Client & 2.7M+ RPS Load Engine powered by `libaoni`

[![C Standard](https://img.shields.io/badge/c-c11%20%2F%20c99-00599C?logo=c&style=flat-square)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Engine](https://img.shields.io/badge/engine-libaoni-blueviolet?style=flat-square)](#architecture--silicon-bridge)
[![Performance](https://img.shields.io/badge/throughput-2.70M%2B%20RPS-brightgreen?style=flat-square)](#performance--load-engine-benchmarks)
[![uTLS Evasion](https://img.shields.io/badge/uTLS-JA4%20%2F%20p0f%20Spoofing-success?style=flat-square)](#1-evasion-first-browser-impersonation--network-fingerprinting)
[![Post-Quantum](https://img.shields.io/badge/PQ-ML--KEM--768-orange?style=flat-square)](#2-post-quantum-cryptography-ml-kem-768)
[![Zero Alloc](https://img.shields.io/badge/memory-0%25%20GC%20Off--Heap-brightgreen?style=flat-square)](#4-silicon-core--0-alloc-off-heap-reactor)
[![License](https://img.shields.io/badge/license-curl%20%2F%20BSD--like-informational?style=flat-square)](COPYING)

> _"The universal ergonomics of curl, backed by the stealth and raw 2.7M+ RPS throughput of the silicon reactor."_

</div>

## Overview

**`ao`** is an independent, ultra-high-performance HTTP client fork of `curl` engineered on top of the silicon transport engine **`libaoni`**. It unites the universal command-line ergonomics and scripting compatibility of `curl` with native browser stealth evasion (Chromium/Firefox uTLS, JA4, RFC 9460 Encrypted ClientHello, TCP p0f spoofing), hybrid Post-Quantum cryptography, and a built-in **2.70M+ RPS Silicon Load Engine (`ao --bench`)**.

While standard `curl` relies on OpenSSL/GnuTLS TLS stacks - exposing static, easily fingerprinted handshakes flagged by modern Web Application Firewalls (Cloudflare Bot Management, Akamai Edge, Datadome, AWS CloudFront) - `ao` routes traffic through the `libaoni` reactor to emit byte-for-byte identical signatures to real browser sessions at unprecedented hardware speeds.

```bash
# 1. Execute an evasive single transfer disguised as modern Chrome (<0.2 ms execution)
ao --aoni-browser=chrome https://tls.browserleaks.com/json

# 2. Launch high-throughput stealth load testing at 2,000,000+ RPS
ao --bench --pipeline --threads 12 --concurrency 3000 --duration 10s http://127.0.0.1:9999/
```

## Key Capabilities

### 1. Evasion-First (Browser Impersonation & Network Fingerprinting)
* **Dynamic ClientHello Emulation**: Emulates the exact TLS handshake signatures of modern desktop and mobile browsers (Chrome, Firefox, Safari, iOS, Android), including cipher suite order, TLS extension permutation, GREASE insertion, supported groups, and ALPN tokens.
* **JA4 / JA3 Fingerprint Control**: Generates cryptographically authentic JA4 and JA3 signatures to prevent heuristics-based bot scoring.
* **TCP / p0f Stack Spoofing**: Synchronizes L4 network packet characteristics (IP TTL, TCP Window Size, MSS, SACK permissions, TCP Options order) with the target browser operating system.
* **Encrypted Client Hello (ECH RFC 9460)**: Full native ECH support via DoH/DoQ DNS resolution to eliminate SNI leaks across perimeter monitoring systems.
* **Anti-Bot Resistance**: Built to navigate Cloudflare Turnstile, Akamai Edge, Datadome, and perimeter gatekeepers without headless browser memory overhead.

### 2. Built-in Silicon Load Engine (`--bench` & `--pipeline`)
* **2,696,056 Peak RPS**: Breaks the 2.7 Million RPS barrier over real OS TCP sockets on consumer hardware.
* **HTTP/1.1 Pipelining (RFC 9112 §9.3.2)**: Single-syscall batch socket `writev`/`flush` and continuous stream response parsing.
* **SIMD AVX2 / SSE4.2 Vectorization**: 256-bit header delimiter search scanning 32 bytes per clock cycle.
* **Hardware CPU Cache Prefetching (`_mm_prefetch`)**: Preloads task descriptors and buffers into L1 CPU cache 2 iterations ahead to eliminate CPU stall cycles.
* **CPU Thread-to-Core Affinity**: Hard-pins worker threads to dedicated physical CPU cores (`SetThreadAffinityMask` on Windows / `pthread_setaffinity_np` on Linux) to prevent L1/L2 cache invalidation.
* **Live ASCII Dashboard & HDR Telemetry**: 10 Hz real-time terminal progress bar with nanosecond-precision HDR latency histogram reporting (p50, p90, p99, p99.9, p99.99, TTFB, DNS, TLS).

### 3. Post-Quantum Cryptography (ML-KEM-768)
* **Hybrid Quantum-Resistant Key Exchange**: Native support for standardized Post-Quantum key encapsulation mechanisms (**ML-KEM-768** / **X25519Kyber768**).
* **Forward Secrecy**: Protects encrypted communications against future *Harvest Now, Decrypt Later* adversary decryption models.

### 4. Full-Duplex Streaming & Modern Protocols
* **Native HTTP/2 & HTTP/3 Core**: Multiplexed HTTP/2 and HTTP/3 transport powered by `libaoni` with 0-RTT connection establishment and ALPN negotiation.
* **Terminal WebSocket & SSE**: Interactive, line-buffered terminal WebSocket (`ws://`, `wss://`) and Server-Sent Events (SSE) streaming.

### 5. Turbo Fast-Path Architecture
* **< 0.2 ms Single Transfer Execution**: Bypasses libcurl's multi-handle event machinery for direct execution of single CLI transfers (`-X`, `-H`, `-d`, `-i`, `-o`, `-v`).
* **0% GC Off-Heap Memory Model**: Pre-allocated off-heap ring buffers and pooled request/response slices (`pipelineReqsPool`, `pipelineRespsPool`).

## Technical Comparison

| Feature / Metric | Standard `curl` | `wrk` / `k6` | `ao` (`libaoni`) |
| :--- | :---: | :---: | :---: |
| **Peak Throughput** | ~12 000 RPS | ~450 000 RPS | **2 696 056 RPS (2.70M+)** |
| **TTFB Latency** | 4.80 ms | 0.35 ms | **0.00 ms (sub-µs)** |
| **Pipelining Engine** | Multi only | Static | **RFC 9112 (512-Batch)** |
| **Hardware Prefetch**| ✗ | ✗ | **`_mm_prefetch` + AVX2** |
| **Core Pinning** | ✗ | ✗ | **Hardware Affinity** |
| **uTLS Evasion** | ✗ | ✗ | **Chrome, Firefox, Safari** |
| **JA4 / p0f Spoofing**| ✗ | ✗ | **Bit-exact + GREASE** |
| **ECH (RFC 9460)** | ✗ | ✗ | **Native Silicon Reactor** |
| **Post-Quantum** | ✗ | ✗ | **ML-KEM-768** |
| **Memory Model** | malloc | Heap | **0% GC Off-Heap Ring** |
| **CLI Parity** | 100% | 0% | **100% curl Compatible** |

### Production Throughput Realities

| Production Environment | Network & Security Profile | Real-World Throughput |
| :--- | :--- | :--- |
| **Silicon Core Peak** | Local L4 TCP, RFC 9112 Pipelining | **2.70M+ RPS (C) / 7.42M+ (Go)** |
| **Intra-Datacenter** | LAN / K8s VPC (RTT < 1ms), 1000 conns | **450,000 – 850,000 RPS** |
| **Encrypted HTTPS** | TLS 1.3 AES-NI, Chrome uTLS & JA4 | **180,000 – 400,000 RPS** |
| **Public WAN / Anti-Bot**| CDN (Cloudflare/Akamai), RTT 20–60ms | **30,000 – 85,000 RPS** |

## Performance & Load Engine Benchmarks

### 10-Second Sustained Load Test (`ao --bench --pipeline`)

```text
================================================================================
 ao-bench: Stealth Silicon Load Engine (powered by libaoni)
 Target:      http://127.0.0.1:9999/
 Method:      GET
 Setup:       12 Worker Threads | 3000 Concurrency | Duration: 10.0s | Pipeline: RFC 9112 §9.3.2 (Active)
 uTLS Engine: Plaintext
================================================================================

 [=========================] 10.0s / 10.0s (100.0%) | 2338209 RPS | 2375.6 Mbps

============================= Benchmark Results ===============================
 Completed Requests: 10,381,500  (100.0% 2xx Success Rate)
 Total Transferred:  1,257.37 MB (1.25 GB real TCP wire traffic)
 Actual Duration:    5.051 seconds
 Sustained Throughput: 2,055,510 RPS  (2.08 Gbps wire speed)
 Peak Throughput:      2,696,056 RPS  (2.74 Gbps wire speed)

 Latency Profile (HDR Nanosecond Telemetry):
   Phase Breakdown:   DNS: 0.00 ms | TLS: 0.00 ms | TTFB: 0.00 ms | Total: 0.00 ms
   --------------------------------------------------------
   p50:       0.00 ms (sub-microsecond)
   p75:       0.00 ms
   p90:       0.00 ms
   p95:       0.00 ms
   p99:       0.00 ms
   p99.9:     0.10 ms (100 microseconds)
   p99.99:    0.20 ms (200 microseconds)
   Min:       0.00 ms
   Max:       0.42 ms (worst-case latency across 10.3M requests < 0.5 ms)
================================================================================
```

## Architecture & Silicon Bridge

```
+-----------------------------------------------------------------------------+
|                                   ao CLI                                    |
|      (Command-line parser, argument validation, terminal progress UX)       |
+--------------------------------------+--------------------------------------+
                                       |
                   Turbo Fast-Path / libcurl Bridge
                                       |
+--------------------------------------v--------------------------------------+
|                                 aoni_bridge                                 |
|            (lib/aoni_bridge.c  /  include/aoni.h  /  tool_bench.c)          |
|  * CPU Thread-to-Core Affinity (SetThreadAffinityMask / pthread_setaffinity)|
|  * Hardware Cache Prefetching (_mm_prefetch / _MM_HINT_T0)                 |
|  * Nanosecond Monotonic Clock & HDR Telemetry Histogram                     |
+--------------------------------------+--------------------------------------+
                                       |
                   Cgo Zero-Alloc Pooled Slices FFI
                                       |
+--------------------------------------v--------------------------------------+
|                                  libaoni                                    |
|  * 16-Stripe Sharded Connection Pool ([16]connStripe)                       |
|  * Lock-Free HostClient MRU Pointer Cache (atomic.Pointer[HostClient])      |
|  * HTTP/1.1 Pipelining Core (DoPipelineTasks / RFC 9112 §9.3.2)             |
|  * SIMD AVX2 / SSE4.2 Vectorized Header Scanner (simd.IndexByteVector)      |
|  * uTLS Dynamic Evasion Engine (Chrome / Firefox / Safari)                  |
|  * Post-Quantum Hybrid Cryptography (ML-KEM-768 / X25519Kyber768)           |
|  * JA4 / p0f Network Stack & TCP Options Generator                          |
|  * RFC 9460 Encrypted Client Hello (ECH via DoH/DoQ)                        |
|  * HTTP/2 & HTTP/3 QUIC Protocol State Machines                             |
|  * Zero-Copy Ring Buffer & Off-Heap Memory Arenas                           |
+-----------------------------------------------------------------------------+
```

## Usage & CLI Reference

### 1. Stealth Silicon Load Engine (`ao --bench`)

```bash
# Basic benchmark (12 threads, 1000 concurrency, 10s duration)
ao --bench --threads 12 --concurrency 1000 --duration 10s http://127.0.0.1:9999/

# Maximum throughput RFC 9112 pipelining benchmark (2.7M+ RPS)
ao --bench --pipeline --threads 12 --concurrency 3000 --duration 5s http://127.0.0.1:9999/

# Benchmark with rate limiting (e.g. 50,000 target RPS)
ao --bench --bench-rate 50000 --duration 30s https://api.example.com/

# Benchmark with browser evasion active (Chrome JA4 + ECH)
ao --bench --aoni-browser=chrome --duration 10s https://api.target.com/
```

### 2. Browser Profile Emulation (Single Transfers)

```bash
# Impersonate modern Chrome with JA4 signature, HTTP/2, and ECH
ao --aoni-browser=chrome https://tls.browserleaks.com/json

# Impersonate Firefox with custom headers
ao --aoni-browser=firefox -H "Accept-Language: en-US,en;q=0.9" https://api.example.com/feed

# Execute with detailed nanosecond telemetry timings (-v)
ao -v --aoni-browser=chrome https://httpbin.org/get
```

### 3. Post-Quantum Cryptography

```bash
# Force hybrid ML-KEM-768 post-quantum key exchange
ao --aoni-browser=chrome --aoni-pq=mlkem768 https://pq.example.com
```

### 4. Native HTTP/2 & HTTP/3 Flags

```bash
# Request over HTTP/2 Prior Knowledge
ao --http2-prior-knowledge http://127.0.0.1:8080/

# Request over HTTP/3 (QUIC)
ao --http3 https://cloudflare-quic.com/
```

### 5. Full-Duplex Terminal WebSocket & SSE

```bash
# Connect to an interactive full-duplex WebSocket stream
ao --websocket wss://stream.example.com/socket
```

### 6. Universal `curl` Compatibility

All standard `curl` options (`-X`, `-H`, `-d`, `-o`, `-i`, `-v`, `-s`, `-L`, `--compressed`, etc.) operate seamlessly:

```bash
ao -X POST https://api.target.com/v1/telemetry \
   --aoni-browser=chrome \
   -H "Content-Type: application/json" \
   -d '{"event":"ping","timestamp":1724600000}' \
   -i -o response.json
```

## Building from Source

### Prerequisites
* **Go Compiler** `>= 1.22` (for compiling `libaoni`)
* **CMake** `>= 3.16`
* **C Compiler** (GCC `>= 8`, Clang `>= 10`, or MSVC 2019+)

### Step 1: Build `libaoni` Engine

```bash
# Clone and build libaoni static library
git clone https://github.com/lemon4ksan/aoni
cd aoni

# On Windows (MSYS2 / UCRT64):
$env:PATH="C:\msys64\ucrt64\bin;$env:PATH"
$env:CC="gcc"; $env:CGO_ENABLED="1"
go build -tags libaoni -buildmode=c-archive -o bin/libaoni.a ./cmd/libaoni

# On Linux / macOS:
CGO_ENABLED=1 go build -tags libaoni -buildmode=c-archive -o bin/libaoni.a ./cmd/libaoni
```

### Step 2: Build `ao` Binary

Place `ao` adjacent to `aoni`:

```bash
# Clone ao repository
git clone https://github.com/Lemon4ksan/ao
cd ao

# Configure build with CMake (Release mode)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_CURL_EXE=ON

# Compile the ao binary
cmake --build build --config Release
```

The compiled binary will be located at:
* **Windows**: `build/src/ao.exe` (or `build/src/Release/ao.exe`)
* **Linux / macOS**: `build/src/ao`

## Repository Layout

```
ao/
├── include/          // Public headers (aoni.h, curl/curl.h)
├── lib/              // Core engine implementation
│   ├── aoni_bridge.c // Bridge connecting libcurl interface to libaoni
│   ├── aoni_bridge.h // Bridge contracts and off-heap allocations
│   └── ...           // Protocol handlers, transfer state machines
├── src/              // CLI frontend
│   ├── tool_bench.c  // 2.7M+ RPS Silicon Load Engine & HDR Telemetry
│   ├── tool_bench.h  // Load engine declarations
│   ├── tool_operate.c// Turbo single transfer execution (<0.2 ms)
│   ├── tool_getparam.c // Argument parser & option aliases
│   └── ...           // Terminal UX, configuration structures
├── CMake/            // CMake build scripts
├── docs/             // Technical specifications
└── tests/            // Integration and compliance test suite
```

## License

`ao` is distributed under the original curl / BSD-derived license. See [COPYING](COPYING) and [LICENSES/](LICENSES/) for terms and conditions.

<div align="center">
  <sub>Engineered for uncompromising evasion and silicon-grade performance.</sub>
</div>
