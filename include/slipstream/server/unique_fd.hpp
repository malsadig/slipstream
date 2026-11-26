#pragma once

#include <utility>
#include <unistd.h>

namespace slipstream::server {
    class UniqueFd {
    public:
        UniqueFd() noexcept = default;

        explicit UniqueFd(int fd) noexcept : fd_(fd) {}

        ~UniqueFd() noexcept {
            if (fd_ >= 0) {
                ::close(fd_);
            }
        }

        UniqueFd(const UniqueFd&) = delete;
        UniqueFd& operator=(const UniqueFd&) = delete;

        UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

        UniqueFd& operator=(UniqueFd&& other) noexcept {
            if (this != &other) {
                if (fd_ >= 0) {
                    ::close(fd_);
                }

                fd_ = std::exchange(other.fd_, -1);
            }
            return *this;
        }

        // Only borrows. Ownership stays with this object. Callers must not close its result
        [[nodiscard]] int get() const noexcept {
            return fd_;
        }
    private:
        int fd_{-1};
    };
}