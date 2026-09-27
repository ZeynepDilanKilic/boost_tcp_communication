# boost_tcp_communication

A C++20 / Boost.Asio TCP echo demo with bounded messages, concurrent client
sessions, automatic client reconnection, operation deadlines and JSON RTT statistics.

The project demonstrates how a TCP byte stream becomes a reliable *framed*
request/response exchange. It is a small portfolio project with reproducible
loopback integration tests.

## Build

Requirements: a C++20 compiler, CMake 3.20+, Boost 1.70+ headers, and Python 3.8+
for integration tests. Boost.System runs header-only; timers use `std::chrono`.
The bundled `nlohmann-json/json.hpp` is used for configuration and reporting.

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-dev python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Configure from the repository root, rather than from the two executable folders.
To build without the Python tests, add `-DBUILD_TESTING=OFF`.
For a non-system Boost installation, add `-DBOOST_ROOT=/path/to/boost`.

On Windows, use a C++20-capable Visual Studio installation and Boost headers:

```powershell
cmake -S . -B build -DBOOST_ROOT=C:/local/boost_1_85_0
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The Boost path is an example: replace it with your installation. Multi-configuration
builds place executables in `build/bin/Release/`, with the `.exe` extension.
Windows socket libraries are linked only on Windows. The automated workflow runs
on Linux; the Windows instructions require validation on a Windows machine.

## Run

Start the server in one terminal:

```bash
./build/bin/TCP_Server 12345 TCP_Server/ServerConfig.json
```

Start a client in another:

```bash
./build/bin/TCP_Client 127.0.0.1 12345 10 1000 64 3000
```

| Program | Positional arguments, in order |
| --- | --- |
| `TCP_Server` | `port` (12345), optional configuration file |
| `TCP_Client` | `host` (127.0.0.1), `port` (12345), `count` (10), `interval_ms` (1000), `payload_bytes` (64), `timeout_ms` (3000) |

Both support `--help`. Client `count=0` continues until Ctrl+C. Server `port=0`
asks the operating system for a free port, printed on startup. The server listens
on all IPv4 interfaces. The client accepts a hostname or IP address.

The server defaults to five active sessions and a 5000 ms frame deadline if no
configuration file is supplied. `maxConnectionLimit` must be an integer from 1
to 10000; `timeoutDuration` must be an integer from 1 to 3600000 milliseconds.
An explicitly supplied missing or malformed configuration fails with a diagnostic.
The old Windows error-number setting `restartOnErrors` is no longer used: a peer
failure closes its session while the listener stays available.

## Reconnection demo

1. Run the server, then run a continuous client:
   `./build/bin/TCP_Client 127.0.0.1 12345 0 250`.
2. Stop the server with Ctrl+C. The client reports failures on stderr and retries
   after 100, 200, 400, 800, 1600, then at most 2000 ms.
3. Restart the same server command. The client reconnects and continues.
4. Stop the client with Ctrl+C to print its JSON statistics.

Retries continue until completion or interruption. A successful echoed message
resets the backoff. DNS resolution/connect and each complete frame read/write
have deadlines, so a connected but silent peer cannot block the client forever.
The server's read deadline includes idle time between frames: choose a timeout
longer than the client's message interval.

## Measure RTT

For a finite run without intentional inter-message delays:

```bash
./build/bin/TCP_Client 127.0.0.1 12345 10000 0 256 3000 > results.json
```

Run clients in separate terminals to exercise concurrent sessions.
Stdout contains one JSON object; connection diagnostics go to stderr.

- `messages`: successful, byte-for-byte validated echoes.
- `connections`, `reconnects`, `failures`: connection/exchange counters.
- `latency_ms`: min, mean, p50, p95, p99 and max round-trip time, measured with
  `steady_clock` from sending a request to receiving its complete echo.
- `sample_count`: number of retained latency samples. At most the latest 10000
  successful samples are retained, including in continuous mode.
- `messages_per_second`: total successful messages divided by wall-clock run time;
  it includes initial connection, configured intervals and reconnect delays.

Percentiles use the nearest-rank method. Failed attempts are counted separately
and excluded from successful-exchange RTT. Each client has one outstanding request,
so these numbers describe sequential echo performance. They are not a saturated
network-bandwidth measurement or a hard real-time guarantee. Record the compiler,
build type, CPU, OS, payload size and client count when sharing results.

## Protocol and design decisions

A frame is a four-byte **unsigned big-endian** payload length followed by that
many bytes. Payloads may contain arbitrary bytes, including NUL, and range from
0 to 1048576 bytes (1 MiB). The server echoes the exact payload.

`async_read` and `async_write` consume complete headers/bodies even when the stream
splits a frame across reads or combines several frames. Lengths are validated
before allocation. A partial frame, malformed length or timeout closes only the
affected connection. This framing intentionally replaces the original native
`size_t` header; both peers must be upgraded together.

Each server session owns its socket, timer and message buffers. A single
`io_context` thread services all sessions. Only one read or write is outstanding
per connection, and the server reads the next frame after completing its echo.
This bounds application buffering and lets TCP flow control handle a slow peer.
Extra connections beyond the configured active-session limit are closed.
`TCP_NODELAY` avoids delayed small-message writes in the echo measurement.

The client retries the interrupted echo after reconnecting. An echo has no lasting
side effects; applying this pattern to payments or commands would require request
IDs and server-side deduplication for stronger delivery semantics. The demo does
not promise exactly-once delivery or provide TLS/authentication.

Cancellation generations prevent old timers and connection attempts from acting
on a later attempt. Call public lifecycle methods on the same thread as
`io_context::run()`. Linux SIGINT/SIGTERM and console Ctrl+C stop pending operations.

## Verification

`ctest` runs protocol/statistics unit checks and Python tests using actual loopback
sockets. They cover fragmented headers and bodies on both peers, concatenated and
empty frames, maximum/oversized lengths, simultaneous clients and connection-limit
reuse, partial-frame timeouts, client-before-server startup, server restart,
silent-peer timeout, numeric/configuration validation and interruption.

Optional memory/undefined-behaviour checks with GCC or Clang on Linux:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DTCP_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

The GitHub Actions workflow builds and tests both Release and sanitizer configurations.

## References

- [Boost.Asio asynchronous operations](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/model/async_ops.html)
- [Boost.Asio streams and partial reads/writes](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/core/streams.html)
