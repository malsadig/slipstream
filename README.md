# Slipstream

I'm building Slipstream in C++23 to evaluate historical trades for one symbol against a rolling volume-weighted average price (VWAP), with limits on trade size and participation. In this exercise, the quotes come from the market and the trades belong to a single firm. VWAP will be calculated from quotes.

The server will check quotes against VWAP and trades against those quotes, then return an accepted or rejected `NewOrder` response after warm-up. An execution report will show how the accepted trades compared with VWAP.

## Where it stands

I've implemented the M1 codec and both CSV replay clients. The codec supports `Quote`, `Trade`, `Heartbeat`, `SessionControl`, and `NewOrder`, including frames arriving in chunks. The clients currently read and write files; there is no TCP server or trade decision logic yet.

- **M2:** TCP server, best bid and ask for the selected symbol, and rolling VWAP.
- **M3:** Price, size, and participation checks; order responses, session control, heartbeats, and an execution report.
- **M4:** Measure trade-receive-to-order-send latency at p50, p99, and p99.9. Optionally compare TCP with gRPC or UDP multicast using the same decision code.

## Build and run the tests

You'll need a compiler and standard library that support C++23, along with CMake 3.20 or newer. There are no third-party libraries to install. Tested on x86_64 Linux with GCC.

The tests use `assert`, so build in Debug with assertions enabled. Run these from the repo root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run the clients

These commands replay the supplied `QandT.csv` without waiting between rows:

```bash
./build/md_client QandT.csv build/quotes.bin --no-wait
./build/oe_client QandT.csv build/trades.bin /dev/null --no-wait
```

The full file produces 6,905 quotes and 3,095 trades across all symbols. Each client writes binary frames to its output file and prints the source rows and final count to stderr. Remove `--no-wait` to preserve the timing between CSV rows.

`/dev/null` supplies no order responses. You can replace it with a binary `NewOrder` file, which the order-entry client reads and validates before replaying trades. It doesn't display or act on those responses yet.

## A few design choices

- The codec reads and writes each field individually, making byte order explicit. Integers are little-endian, and prices use integers scaled by 10,000 so decimal prices can be represented exactly to four places.
- The stream decoder keeps incomplete frames between calls and returns messages that own their data. Callers can reuse their input buffers, but the decoder copies bytes and allocates through vectors. There are no latency measurements yet.
- A malformed frame stops the decoder for the rest of its lifetime. It returns any valid messages decoded before the error, but makes no attempt to find the next frame boundary and recover the stream.

The clients accept plain, unquoted CSV and each use their own replay start time. The [replay notes](docs/replay.md) explain timestamps, trade IDs, and the current limitations. The [wire format and codec reference](docs/protocol.md) covers message layouts, and API behavior.

## Tests

`codec_tests` checks fixed expected-byte fixtures, round trips, malformed input, and truncated frames. For all five message types, I test every possible two-chunk split, along with one-byte feeds and multiple frames in a single chunk.

`csv_tests` currently checks field splitting only. Full CSV parsing, replay timing, and order-response handling still need automated coverage.

## Where things are

- [Public headers](include/slipstream/) and [codec implementation](src/codec/)
- [CSV parsing and replay](src/replay/) and [client entry points](src/clients/)
- [Codec tests](tests/codec/) and [CSV tests](tests/replay/)
