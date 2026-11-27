# CSV replay

[Back to the README](../README.md)

The market-data client emits `Quote` frames for every quote row, and the order-entry client emits `Trade` frames for every trade row. Both replay all symbols in the file. The server filters for the configured symbol.

## CSV input

The reader accepts plain, unquoted CSV with the same ten-column header as `QandT.csv`. It skips empty lines and lines starting with `#`, including the descriptions above the header. The whole file is loaded into memory before replay.

Prices are parsed directly into signed integers scaled by 10,000, with at most four decimal places. Symbols must be 1–12 bytes and are null-padded when shorter than 12 bytes. Timestamps use `HH:MM:SS.mmm` and must be nondecreasing across all data rows; a replay cannot cross midnight.

## Timing and IDs

Each row's offset is measured from the first data row in the full CSV, before selecting quotes or trades. Message timestamps are the replay start time since the Unix epoch plus that offset. Trade IDs are one-based positions among all data rows, including quotes; comments and the header do not count.

Pacing uses a steady clock and each row's offset. In file mode, each client establishes its own startup time; `--no-wait` skips the waits without changing the timestamp calculation. TCP mode requires the same future `--start-at-ns` value for both clients and is always paced. Both paced clients wait through the full CSV timeline, including rows of the other type, before finishing.

## Output and responses

In file mode, the clients write binary frames to the supplied output paths. TCP mode uses `<input.csv> --tcp <host> <port> --start-at-ns <unix-ns>` and sends frames to the corresponding server port; start the server first. Source-row logs and the final emitted count go to stderr. The supplied file produces 6,905 quotes and 3,095 trades.

In file mode, the order-entry client reads and validates its entire response file before replaying trades. That file must contain only complete `NewOrder` frames; `/dev/null` provides an empty response stream. Decoded responses are stored in memory but are not displayed or used to change replay behavior. TCP mode does not read a response file; live response handling is deferred to M3.
