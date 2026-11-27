#pragma once

#include "slipstream/server/report_queue.hpp"
#include "slipstream/server/unique_fd.hpp"
#include "slipstream/server/report_sink.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <system_error>
#include <stdexcept>
#include <iomanip>
#include <exception>
#include <string_view>

#include <sys/eventfd.h>

namespace slipstream::server {
    class AsyncReporter final : public IReportSink {
    public:
        AsyncReporter(std::size_t capacity, std::ostream& output):
            queue_(capacity), output_(output), failure_event_(createFailureEvent()),
            worker_([this] { run(); }) {}

        ~AsyncReporter() noexcept override {
            (void) finish();
        }

        void publish(const ReportSnapshot& report) override {
            if (failed()) {
                throw std::runtime_error("Report printer failed.");
            }

            if (!queue_.tryPush(report)) {
                throw std::runtime_error("Report queue capacity exceeded.");
            }

            wake();
        }

        [[nodiscard]] bool finish() noexcept {
            done_.store(true, std::memory_order_release);
            wake();

            if (worker_.joinable()) {
                worker_.join();
            }

            return !failed();
        }

        [[nodiscard]] bool failed() const noexcept {
            return failed_.load(std::memory_order_acquire);
        }

        [[nodiscard]] int failureFd() const noexcept {
            return failure_event_.get();
        }
        
    private:
        static UniqueFd createFailureEvent() {
            const int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (fd == -1) {
                const int error = errno;
                throw std::system_error(error, std::generic_category(), "eventfd");
            }
            return UniqueFd{fd};
        }

        void wake() noexcept {
            generation_.fetch_add(1, std::memory_order_release);
            generation_.notify_one();
        }

        void markFailed() noexcept {
            failed_.store(true, std::memory_order_release);

            int result;
            do {
                result = ::eventfd_write(failure_event_.get(), 1);
            } while (result == -1 && errno == EINTR);

            if (result == -1 && errno != EAGAIN) {
                std::terminate();
            }
        }

        void flushOutput() {
            output_.flush();
            if (!output_) {
                throw std::runtime_error("Report output failed");
            }
        }

        void writeReport(const ReportSnapshot& report) {
            std::string_view symbol {
                report.symbol.data(), report.symbol.size()
            };

            symbol = symbol.substr(0, symbol.find('\0'));

            output_ << std::dec << "ts_ns=" << report.ts_ns << " symbol=" <<
                symbol << " window_quotes=" << report.window_quotes << " vwap=";

            if (!report.has_vwap) {
                output_ << "NA";
            } else {
                const auto magnitude = report.vwap_px < 0
                    ? std::uint64_t{0} - static_cast<std::uint64_t>(report.vwap_px)
                    : static_cast<std::uint64_t>(report.vwap_px);
                
                if (report.vwap_px < 0) {
                    output_ << "-";
                }

                output_ << magnitude / 10000 << "."
                    << std::setfill('0') << std::setw(4)
                    << magnitude % 10000
                    << std::setfill(' ');
            }

            output_ << '\n';
            flushOutput();
        }

        void drain() {
            ReportSnapshot report{};
            while (queue_.tryPop(report)) {
                writeReport(report);
            }
        }

        void run() noexcept {
            try {
                for (;;) {
                    const auto observed = generation_.load(std::memory_order_acquire);

                    drain();

                    if (done_.load(std::memory_order_acquire)) {
                        drain();
                        flushOutput();
                        return;
                    }

                    generation_.wait(observed, std::memory_order_acquire);
                }
            } catch (...) {
                markFailed();
            }
        }

        ReportQueue queue_;
        std::ostream& output_;
        UniqueFd failure_event_;
        std::atomic<std::uint64_t> generation_{0};
        std::atomic<bool> done_{false};
        std::atomic<bool> failed_{false};

        std::thread worker_;
    };
}