# CSV replay

[Back to the README](../README.md)

The market-data client emits `Quote` frames for every quote row, and the order-entry client emits `Trade` frames for every trade row. Both replay all symbols in the file. Filtering for one symbol belongs to the planned server.

## CSV input

The reader accepts plain, unquoted CSV with the same ten-column header as `QandT.csv`. It skips empty lines and lines starting with `#`, including the descriptions above the header. The whole file is loaded into memory before replay.

Prices are parsed directly into signed integers scaled by 10,000, with at most four decimal places. Symbols must be 1–12 bytes and are null-padded when shorter than 12 bytes. Timestamps use `HH:MM:SS.mmm` and must be nondecreasing across all data rows; a replay cannot cross midnight.

## Timing and IDs

Each row's offset is measured from the first data row in the full CSV, before selecting quotes or trades. Message timestamps are the client's replay startup time since the Unix epoch plus that offset. Trade IDs are one-based positions among all data rows, including quotes; comments and the header do not count.

Pacing uses a steady clock and each row's offset. `--no-wait` skips the waits without changing the timestamp calculation. Each client establishes its own startup time, so separately launched clients do not share a synchronized replay start.

## Output and responses

The clients write binary frames to the supplied output paths. Source-row logs and the final emitted count go to stderr. The supplied file produces 6,905 quotes and 3,095 trades.

The order-entry client reads and validates its entire response file before replaying trades. That file must contain only complete `NewOrder` frames; `/dev/null` provides an empty response stream. Decoded responses are stored in memory but are not displayed or used to change replay behavior. TCP communication and live response handling are not implemented yet.
