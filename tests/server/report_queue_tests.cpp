#include "slipstream/server/report_queue.hpp"

#include <cassert>
#include <cstdint>
#include <thread>

using slipstream::server::ReportQueue;
using slipstream::server::ReportSnapshot;

static ReportSnapshot makeReport(std::uint64_t id) {
    ReportSnapshot report{};
    report.ts_ns = id;
    report.symbol = {'S', 'Y', 'N', 'T', 'H', '1'};
    report.window_quotes = id + 1;
    report.vwap_px = -static_cast<std::int64_t>(id);
    report.has_vwap = (id % 2 != 0);
    return report;
}

int main() {
    {
        ReportQueue queue{2};
        ReportSnapshot received{};

        const auto first = makeReport(1);
        const auto second = makeReport(2);
        const auto third = makeReport(3);

        assert(queue.capacity() == 2);
        assert(!queue.tryPop(received));

        assert(queue.tryPush(first));
        assert(queue.tryPush(second));
        assert(!queue.tryPush(third));

        assert(queue.tryPop(received));
        assert(received == first);

        // Reuse space while preserving the unread second report.
        assert(queue.tryPush(third));

        assert(queue.tryPop(received));
        assert(received == second);

        assert(queue.tryPop(received));
        assert(received == third);

        assert(!queue.tryPop(received));

        // Reuse after draining, including another index wrap.
        assert(queue.tryPush(first));
        assert(queue.tryPop(received));
        assert(received == first);
        assert(!queue.tryPop(received));
    }

    {
        ReportQueue queue{3};
        constexpr std::uint64_t total = 1000;

        std::jthread producer{[&] {
            for (std::uint64_t id = 0; id < total; ++id) {
                const auto report = makeReport(id);

                while (!queue.tryPush(report)) {
                    std::this_thread::yield();
                }
            }
        }};

        for (std::uint64_t id = 0; id < total; ++id) {
            ReportSnapshot received{};

            while (!queue.tryPop(received)) {
                std::this_thread::yield();
            }

            assert(received == makeReport(id));
        }

        producer.join();

        ReportSnapshot received{};
        assert(!queue.tryPop(received));
    }
}