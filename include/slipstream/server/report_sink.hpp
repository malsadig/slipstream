#pragma once

#include "slipstream/server/report_queue.hpp"

namespace slipstream::server {
    class IReportSink {
    public:
        virtual ~IReportSink() = default;

        virtual void publish(const ReportSnapshot& report) = 0;
    };
}