#include "slipstream/codec.hpp"
#include "slipstream/messages.hpp"
#include "support/codec_fixtures.hpp"
#include "support/codec_assertions.hpp"

#include <cassert>
#include <array>
#include <variant>
#include <algorithm>

void testValidQuoteEncoding() {
    Quote quote {
        .symbol = "somesymbol",
        .ts_ns = fixtures::timestamp,
        .ask_qty = 100,
        .ask_px = 100000
    };

    std::array<std::byte, kQuoteWireSize> output{};

    EncodeResult result = encode(quote, output);

    assert(result.error == CodecError::NONE);
    assert(result.bytes_written == fixtures::quoteBytes.size());

    assert(output == fixtures::quoteBytes);
}

void testValidQuoteDecoding() {
    DecodeResult result = decodeOne(fixtures::quoteBytes);
    assert(result.status == DecodeStatus::COMPLETE);
    assert(result.error == CodecError::NONE);
    assert(result.bytes_consumed == kQuoteWireSize);
    
    assert(result.message.has_value());

    const Message& message = *result.message;

    const Quote* quote = std::get_if<Quote>(&message);

    assert(quote != nullptr);
    assert(quote->ts_ns == fixtures::timestamp);
    assert(std::ranges::equal(quote->symbol, fixtures::expectedSymbol));
    assert(quote->ask_px == 100000);
    assert(quote->ask_qty == 100);
    assert(quote->bid_px == 0);
    assert(quote->bid_qty == 0);
}

void testQuoteRoundTrips() {
    const Quote basic{
        .symbol = "somenewsymb",
        .ts_ns = fixtures::timestamp,
        .bid_qty = 25,
        .bid_px = 263000,
        .ask_qty = 100,
        .ask_px = 264000
    };

    assertRoundTrip<kQuoteWireSize>(basic);

    Quote negativePrices = basic;
    negativePrices.bid_px = -263000;
    negativePrices.ask_px = -262000;

    assertRoundTrip<kQuoteWireSize>(negativePrices);

    Quote fullSymbol = basic;
    std::copy_n("ABCDEFGHIJKL", kSymbolSize, fullSymbol.symbol);

    assertRoundTrip<kQuoteWireSize>(fullSymbol);
}

void runQuoteTests() {
    testValidQuoteEncoding();
    testValidQuoteDecoding();
    testQuoteRoundTrips();
}