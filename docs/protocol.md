# Wire format and codec

[Back to the README](../README.md)

## Wire format

`MAMD/1` means Mossab Alsadig Market Data, version 1; `MAOE/1` means Mossab Alsadig Order Entry, version 1. The codec reads and writes each field individually. Integers are little-endian, with fields packed together without padding.

Prices use signed 64-bit integers multiplied by 10,000, e.g. `100000` represents `10.0000`. For timestamps, `ts_ns` holds an unsigned 64-bit count of nanoseconds since the Unix epoch.

Each symbol takes exactly 12 bytes. Shorter symbols are null-padded, and a symbol that uses all 12 bytes has no terminating null. The codec copies those bytes as supplied, leaving ASCII content and padding unchecked.

Every frame starts with a four-byte header. `body_len` occupies bytes 0 and 1 as an unsigned 16-bit integer, followed by `msg_type` at byte 2 and `version` at byte 3 (both unsigned 8-bit integers). The body length counts the bytes after the header, so the full frame is `4 + body_len` bytes. The sizes below exclude any padding and include the `id` and `trade_id` fields.

The body fields below are in wire order. `u32` means an unsigned 32-bit integer and `i64` means a signed 64-bit integer, with the same shorthand used for the other widths.

- `Quote` (type `1`, 44-byte body, 48-byte frame): `symbol[12]`, `ts_ns: u64`, `bid_qty: u32`, `bid_px: i64`, `ask_qty: u32`, `ask_px: i64`.
- `Trade` (type `2`, 41-byte body, 45-byte frame): `symbol[12]`, `ts_ns: u64`, `qty: u32`, `px: i64`, `aggressor: char`, `id: i64`.
- `Heartbeat` (type `3`, 8-byte body, 12-byte frame): `ts_ns: u64`.
- `SessionControl` (type `4`, 9-byte body, 13-byte frame): `ts_ns: u64`, `state: u8`.
- `NewOrder` (MAOE/1 type `10`, 50-byte body, 54-byte frame): `client_order_id: u64`, `symbol[12]`, `status: char`, `ts_ns: u64`, `trade_id: i64`, `side: char`, `qty: u32`, `limit_px: i64`.

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

## Codec tests

The tests compare encoded bytes against fixtures whose expected bytes are pre-defined. They also decode those fixtures and compare every field when all five message types are round-tripped. The cases cover negative prices and null-padded symbols, along with full-length symbols and valid enum values. NewOrder also has test cases for its status/side combinations and values at the integer boundaries.

Invalid versions and message types get tested alongside incorrect body lengths and enum values. I also check incomplete headers and bodies, plus undersized output buffers and whether output stays unchanged when encoding fails.

For `StreamDecoder`, the fixtures for all five message types, including NewOrder, get split at every possible two-chunk boundary. Other cases feed one byte at a time or put several frames into the same chunk, including chunks that end with an incomplete frame. The tests check the order of the messages and make sure each one appears exactly once, after the whole frame has arrived.

I check that the same error comes back on later calls, including when the failing call also returns earlier valid messages. The `finish()` cases cover an empty buffer and input that ends partway through a header or body.
