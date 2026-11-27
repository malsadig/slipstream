#include "slipstream/server/async_reporter.hpp"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>

#include <poll.h>

using slipstream::server::AsyncReporter;
using slipstream::server::ReportSnapshot;

class FailingFlush final : public std::stringbuf {
protected:
    int sync() override {
        return -1;
    }
};

int main() {
    {
        std::ostringstream output;
        AsyncReporter reporter{2, output};

        assert(reporter.finish());
        assert(reporter.finish()); // Repeated completion is harmless.
        assert(output.str().empty());
    }

    {
        std::ostringstream output;
        AsyncReporter reporter{4, output};

        ReportSnapshot report{
            1,
            {'S', 'Y', 'N', 'T', 'H', '1'},
            2,
            222230,
            true
        };
        reporter.publish(report);

        report.ts_ns = 2;
        report.window_quotes = 0;
        report.has_vwap = false;
        reporter.publish(report);

        report.ts_ns = 3;
        report.window_quotes = 1;
        report.vwap_px = 0;
        report.has_vwap = true;
        reporter.publish(report);

        report.ts_ns = 4;
        report.vwap_px = std::numeric_limits<std::int64_t>::min();
        reporter.publish(report);

        assert(reporter.finish());

        // Inspect the borrowed stream only after the worker has joined.
        assert(output.str() ==
            "ts_ns=1 symbol=SYNTH1 window_quotes=2 vwap=22.2230\n"
            "ts_ns=2 symbol=SYNTH1 window_quotes=0 vwap=NA\n"
            "ts_ns=3 symbol=SYNTH1 window_quotes=1 vwap=0.0000\n"
            "ts_ns=4 symbol=SYNTH1 window_quotes=1 "
            "vwap=-922337203685477.5808\n");
    }

    {
        FailingFlush buffer;
        std::ostream output{&buffer};
        AsyncReporter reporter{1, output};

        reporter.publish(ReportSnapshot{});

        pollfd event{reporter.failureFd(), POLLIN, 0};
        int ready;
        do {
            ready = ::poll(&event, 1, 2000);
        } while (ready == -1 && errno == EINTR);

        assert(ready == 1);
        assert(event.revents & POLLIN);
        assert(reporter.failed());
        assert(!reporter.finish());
    }

    {
        // Final flushing must also detect failure with no reports queued.
        FailingFlush buffer;
        std::ostream output{&buffer};
        AsyncReporter reporter{1, output};

        assert(!reporter.finish());
        assert(reporter.failed());
    }
}