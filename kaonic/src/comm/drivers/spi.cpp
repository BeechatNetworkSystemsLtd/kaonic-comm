#include "kaonic/comm/drivers/spi.hpp"

#include <endian.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "kaonic/common/logging.hpp"

namespace kaonic::drivers {

auto spi::open(const spi_config& config) -> error {
    _device_fd = ::open(config.device_path.c_str(), O_RDWR);
    if (_device_fd < 0) {
        log::error("[SPI] Error {} from open: {}", errno, strerror(errno));
        return error::fail();
    }

    if (ioctl(_device_fd, SPI_IOC_WR_MODE, &config.mode) == -1) {
        log::error("[SPI] Error {} from ioctl (set mode): {}", errno, strerror(errno));
        close();
        return error::fail();
    }

    if (ioctl(_device_fd, SPI_IOC_WR_BITS_PER_WORD, &config.bits_per_word) == -1) {
        log::error("[SPI] Error {} from ioctl (set bits per word): {}", errno, strerror(errno));
        close();
        return error::fail();
    }

    if (ioctl(_device_fd, SPI_IOC_WR_MAX_SPEED_HZ, &config.speed) == -1) {
        log::error("[SPI] Error {} from ioctl (set speed): {}", errno, strerror(errno));
        close();
        return error::fail();
    }

    return error::ok();
}

auto spi::read_buffer(const uint16_t addr, uint8_t* buffer, size_t length) -> error {
    if (!buffer || length == 0) {
        log::error("[SPI] Invalid buffer or length for read_buffer.");
        return error::invalid_arg();
    }

    const auto write_reg = htobe16(addr);

    struct spi_ioc_transfer transfers[2] = {};

    transfers[0].tx_buf = reinterpret_cast<__u64>(&write_reg);
    transfers[0].rx_buf = 0;
    transfers[0].len = sizeof(write_reg);

    transfers[1].tx_buf = 0;
    transfers[1].rx_buf = reinterpret_cast<__u64>(buffer);
    transfers[1].len = length;

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(2), transfers);
    if (retv < 0) {
        log::error("[SPI] Error {} from ioctl (read buffer): {}", errno, strerror(errno));
        return error::fail();
    }

    return error::ok();
}

auto spi::write_buffer(const uint16_t addr, const uint8_t* buffer, size_t length) -> error {
    if (!buffer || length == 0) {
        log::error("[SPI] Invalid buffer or length for read_buffer.");
        return error::invalid_arg();
    }

    const auto write_reg = htobe16(addr);
    _tx_buffer.resize(sizeof(write_reg) + length);

    memcpy(_tx_buffer.data(), &write_reg, sizeof(write_reg));
    memcpy(_tx_buffer.data() + sizeof(write_reg), buffer, length);

    struct spi_ioc_transfer transfer = { 0 };
    transfer.tx_buf = reinterpret_cast<__u64>(_tx_buffer.data());
    transfer.rx_buf = 0;
    transfer.len = _tx_buffer.size();

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(1), &transfer);
    if (retv < 0) {
        log::error("[SPI] Error {} from ioctl (write buffer): {}", errno, strerror(errno));
        return error::fail();
    }

    return error::ok();
}

auto spi::close() noexcept -> void {
    ::close(_device_fd);
}

} // namespace kaonic::drivers
