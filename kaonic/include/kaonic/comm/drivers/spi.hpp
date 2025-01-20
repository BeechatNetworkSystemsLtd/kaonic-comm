#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "kaonic/common/error.hpp"

namespace kaonic::drivers {

struct spi_config final {
    std::string device_path;
    uint32_t speed;
    uint8_t mode;
    uint8_t bits_per_word;
};

class spi final {

public:
    explicit spi() noexcept = default;
    ~spi() = default;

    [[nodiscard]] auto open(const spi_config& config) -> error;

    [[nodiscard]] auto read_buffer(const uint16_t addr, uint8_t* buffer, size_t length) -> error;

    [[nodiscard]] auto write_buffer(const uint16_t addr, const uint8_t* buffer, size_t length)
        -> error;

    auto close() noexcept -> void;

protected:
    spi(const spi&) = delete;
    spi(spi&&) = delete;

    spi& operator=(const spi&) = delete;
    spi& operator=(spi&&) = delete;

private:
    int _device_fd;

    std::vector<uint8_t> _tx_buffer;
};

} // namespace kaonic::drivers
