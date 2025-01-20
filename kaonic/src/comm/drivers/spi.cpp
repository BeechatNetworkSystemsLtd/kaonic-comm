#include "kaonic/comm/drivers/spi.hpp"

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

auto spi::read_buffer(const uint8_t addr, uint8_t* buffer, size_t length) -> error {
    if (!buffer || length == 0) {
        log::error("[SPI] Invalid buffer or length for read_buffer.");
        return error::invalid_arg();
    }

    uint8_t tx_buffer[length + 1];
    uint8_t rx_buffer[length + 1];

    tx_buffer[0] = addr | 0x80; // Set MSB to indicate read operation
    std::fill(tx_buffer + 1, tx_buffer + length + 1, 0x00);

    struct spi_ioc_transfer xfer = { 0 };
    xfer.tx_buf = (__u64)tx_buffer;
    xfer.rx_buf = (__u64)rx_buffer;
    xfer.len = (__u32)(length + 1);

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(1), &xfer);
    if (retv < 0) {
        log::error("[SPI] Error {} from ioctl (read buffer): {}", errno, strerror(errno));
        return error::fail();
    }

    for (size_t i = 0; i < length; ++i) {
        buffer[i] = rx_buffer[i + 1];
    }

    return error::ok();
}

auto spi::write_buffer(const uint8_t addr, const uint8_t* buffer, size_t length) -> error {
    if (!buffer || length == 0) {
        log::error("[SPI] Invalid buffer or length for write_buffer.");
        return error::invalid_arg();
    }

    uint8_t tx_buffer[length + 1];

    tx_buffer[0] = addr & 0x7F; // Clear MSB to indicate write operation
    for (size_t i = 0; i < length; ++i) {
        tx_buffer[i + 1] = buffer[i];
    }

    struct spi_ioc_transfer xfer = { 0 };
    xfer.tx_buf = (__u64)tx_buffer;
    xfer.rx_buf = 0;
    xfer.len = (__u32)(length + 1);

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(1), &xfer);
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
