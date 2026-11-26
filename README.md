# Slipstream

The goal of this project is to build a C++23 execution engine that evaluates historical trades for a single ticker using a rolling VWAP calculated from market quotes. Two clients will replay quotes and trades over TCP at first, with the server checking quotes against VWAP (+- some tolerance) and trades against those quotes while enforcing limits on trade size and participation rate. After a brief warm-up, each trade gets an accepted or rejected `NewOrder` response. At the end of a run, I create an execution report showing how the accepted trades compared with VWAP, along with measurements of the time from receiving a trade to sending its `NewOrder` response (tick to order).

I'm building Slipstream in C++23 to encode and decode binary market data and order messages, including data that arrives in chunks. The codec currently supports the four GCMD/1 messages (`Quote`, `Trade`, `Heartbeat`, and `SessionControl`) plus GCOE/1 `NewOrder`. I've implemented both CSV replay clients for M1.

## Milestones

- M1 covers the GCMD/1 and GCOE/1 codecs and the two CSV replay clients, with tests for frames arriving in chunks. The clients preserve the timing between rows in the CSV.
- M2 adds the TCP server so both clients can feed it data. For the selected symbol, it will keep track of the best bid and ask and calculate VWAP from quotes over a rolling window.
- M3 decides whether each trade meets the price rules and the size and participation limits once warm-up is complete. The server sends accepted or rejected `NewOrder` messages and handles session control and heartbeats, then prints an execution report when the run ends.
- M4 measures the time from receiving a trade to sending a `NewOrder`, including p50, p99, and p99.9. There's also an optional gRPC or UDP multicast version that reuses the same decision code.

## Build and run the tests

This project was built and tested locally in C++23 using g++ (GCC) 16.2.1 on Omarchy 4.5.0, an Arch-based Linux system running on x86_64. The project uses CMake 4.4.4, GNU Make 4.4.1, and CTest 4.4.4.

You'll need a compiler and standard library that support C++23, along with CMake 3.20 or newer. There are no third-party libraries to install.

The tests use `assert`, so build in Debug with assertions enabled. Run these from the repo root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CTest runs `codec_tests` and `csv_tests`.

## Run the clients

After building, run each client with the supplied `QandT.csv` and real-time pacing:

```bash
./build/md_client QandT.csv build/quotes.bin
./build/oe_client QandT.csv build/trades.bin /dev/null
```

Add `--no-wait` to either command to skip the delays. The full file produces 6,905 Quotes and 3,095 Trades. `/dev/null` supplies no responses; replace it with a binary NewOrder file to read responses. The clients write binary frames to files and print each source row as it is emitted. TCP transport is not implemented yet.

The reader accepts plain, unquoted CSV and converts prices to integers scaled by 10,000. Message timestamps use each client's replay startup time since the Unix epoch plus the offset from the first data row in the full file. Trade IDs are one-based positions among all data rows, including quotes.

## Where things are

The public headers are in `include/slipstream/`, where `messages.hpp` defines the message types along with their enums and wire sizes. `codec.hpp` encodes and decodes individual frames, while `stream_decoder.hpp` decodes data arriving in chunks.

Under `src/codec/`, `encode.cpp` and `decode.cpp` handle individual frames. The byte helpers are private to this directory (`byte_reader.hpp` and `byte_writer.hpp`), and `stream_decoder.cpp` buffers incoming data until it can extract complete frames.

CSV parsing and replay are in `src/replay/`, and the two client entry points are in `src/clients/`.

The codec tests are in `tests/codec/`, with fixed byte fixtures and shared assertions under `tests/codec/support/`. `test_main.cpp` calls the suites declared in `test_suites.hpp`, and the test cases live in `*_tests.cpp`. CSV tests are in `tests/replay/`.

## A bit about the GCMD/1 and GCOE/1 wire formats

The codec supports version `1` for both protocols and reads/writes each field individually. Integers are little-endian, with fields packed together without padding.

Prices use signed 64-bit integers multiplied by 10,000, e.g. `100000` represents `10.0000`. For timestamps, `ts_ns` holds an unsigned 64-bit count of nanoseconds since the Unix epoch.

Each symbol takes exactly 12 bytes. Shorter symbols are null-padded, and a symbol that uses all 12 bytes has no terminating null. The codec copies those bytes as supplied, leaving ASCII content and padding unchecked.

Every frame starts with a four-byte header. `body_len` occupies bytes 0 and 1 as an unsigned 16-bit integer, followed by `msg_type` at byte 2 and `version` at byte 3 (both unsigned 8-bit integers). The body length counts the bytes after the header, so the full frame is `4 + body_len` bytes.

The body fields below are in wire order. `u32` means an unsigned 32-bit integer and `i64` means a signed 64-bit integer, with the same shorthand used for the other widths.

- `Quote` (type `1`, 44-byte body, 48-byte frame): `symbol[12]`, `ts_ns: u64`, `bid_qty: u32`, `bid_px: i64`, `ask_qty: u32`, `ask_px: i64`.
- `Trade` (type `2`, 41-byte body, 45-byte frame): `symbol[12]`, `ts_ns: u64`, `qty: u32`, `px: i64`, `aggressor: char`, `id: i64`.
- `Heartbeat` (type `3`, 8-byte body, 12-byte frame): `ts_ns: u64`.
- `SessionControl` (type `4`, 9-byte body, 13-byte frame): `ts_ns: u64`, `state: u8`.
- `NewOrder` (GCOE/1 type `10`, 50-byte body, 54-byte frame): `client_order_id: u64`, `symbol[12]`, `status: char`, `ts_ns: u64`, `trade_id: i64`, `side: char`, `qty: u32`, `limit_px: i64`.

For a Trade, `aggressor` must be `B` for buy, `S` for sell, or `?` for unknown. SessionControl uses `0` for open, `1` for halt, and `2` for close in order to control the connected data streams.

NewOrder's `status` must be `A` for accepted or `R` for rejected, and its `side` must be `B` for buy or `S` for sell.

## Encode a message

`encode(message, output)` writes a complete frame, including the header. On success, you get `CodecError::NONE` and the number of bytes written.

If the output buffer is too small, the result is `BUFFER_TOO_SMALL`, including when the message also has an invalid field. With enough space in the buffer, `encode()` returns `INVALID_FIELD` for an invalid Trade aggressor or SessionControl state, or when a NewOrder's status is outside `A`/`R` or its side is outside `B`/`S`. Both errors leave the output unchanged and report zero bytes written.

## Decode a frame

`decodeOne(input)` examines the first frame and, once it's complete and valid, returns `COMPLETE` with an owning `Message`. The `bytes_consumed` value is that frame's size, so any bytes after it remain for the caller to handle.

You get `NEED_MORE` when decoding needs more bytes. If the header or message has an unsupported or invalid value, the result is `ERROR`, with checks for the protocol version and message type as well as the exact body length. The decoder also validates Trade aggressors and SessionControl states, along with NewOrder status (`A`/`R`) and side (`B`/`S`). For either `NEED_MORE` or `ERROR`, the result has no message and `bytes_consumed` is zero.

## Feed it chunks

`StreamDecoder::feed(input)` takes a chunk of bytes and gives you every message completed by that call. If the chunk ends partway through a frame, the decoder keeps those bytes for the next call, including when other complete frames came before it. You can pass several frames in one chunk and get the messages back in order, each exactly once.

Waiting for the rest of a frame returns `CodecError::NONE`. When a later frame in a chunk is malformed, the result includes the valid messages that came before it along with the error.

Once the decoder hits an error, every later `feed()` returns that error with an empty message list. Parsing stays stopped for the rest of that decoder's lifetime, so create a new decoder for a new stream.

An empty `feed({})` supplies zero bytes. Call `finish()` when you know input has ended, since an empty feed doesn't signal that.

`finish()` returns any recorded error, or `CodecError::NONE` if the buffer is empty and error-free. An incomplete frame latches `CodecError::TRUNCATED_INPUT`, so later `feed()` calls keep returning that error with no messages even if the missing bytes arrive. A clean `finish()` leaves the decoder able to accept more input because there is no separate closed state.

## Tests

The tests compare encoded bytes against fixtures whose expected bytes are pre-defined. They also decode those fixtures and compare every field when all five message types are round-tripped. The cases cover negative prices and null-padded symbols, along with full-length symbols and valid enum values. NewOrder also has test cases for its status/side combinations and values at the integer boundaries.

Invalid versions and message types get tested alongside incorrect body lengths and enum values. I also check incomplete headers and bodies, plus undersized output buffers and whether output stays unchanged when encoding fails.

For `StreamDecoder`, the fixtures for all five message types, including NewOrder, get split at every possible two-chunk boundary. Other cases feed one byte at a time or put several frames into the same chunk, including chunks that end with an incomplete frame. The tests check the order of the messages and make sure each one appears exactly once, after the whole frame has arrived.

I check that the same error comes back on later calls, including when the failing call also returns earlier valid messages. The `finish()` cases cover an empty buffer and input that ends partway through a header or body.

If you have any questions, comments, concerns, or improvement ideas - please let me know!
