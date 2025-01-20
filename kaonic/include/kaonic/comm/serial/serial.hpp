#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>

#include "kaonic/common/error.hpp"

namespace kaonic::comm::serial {

struct config final {
    std::filesystem::path tty_path;
    uint32_t bound_rate;
};

class serial {

public:
    explicit serial() noexcept = default;
    ~serial() = default;

    serial(const serial&) = delete;
    serial(serial&&) = delete;

    [[nodiscard]] auto open(const config& config) -> error;

    [[nodiscard]] auto read(uint8_t* data, size_t length, std::chrono::milliseconds timeout)
        -> size_t;

    [[nodiscard]] auto write(const uint8_t* data, size_t length) -> size_t;

    auto close() noexcept -> void;

    serial& operator=(const serial&) = delete;
    serial& operator=(serial&&) = delete;

private:
    int _comport_fd;

    mutable std::mutex _mut;
};

} // namespace kaonic::comm::serial
