#include "kaonic/comm/drivers/spi.hpp"

#include <endian.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "kaonic/common/logging.hpp"

namespace kaonic::drivers {

auto spi::open(const spi_config& config) -> error {

    log::debug("spi: open '{}' device", config.dev);

    _device_fd = ::open(config.dev.data(), O_RDWR);
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

    _config = config;

    return error::ok();
}

auto spi::read_buffer(const uint16_t addr, uint8_t* buffer, size_t length) -> error {

    if (!buffer || length == 0) {
        log::error("spi: invalid buffer or length for read op");
        return error::invalid_arg();
    }

    const auto write_reg = htobe16(addr);

    struct spi_ioc_transfer xfer[2];
    memset(xfer, 0x00, sizeof(xfer));

    xfer[0].tx_buf = reinterpret_cast<__u64>(&write_reg);
    xfer[0].rx_buf = 0x00;
    xfer[0].len = sizeof(write_reg);
    xfer[0].speed_hz = _config.speed;
    xfer[0].bits_per_word = _config.bits_per_word;

    xfer[1].tx_buf = 0x00;
    xfer[1].rx_buf = reinterpret_cast<__u64>(buffer);
    xfer[1].len = length;
    xfer[1].speed_hz = _config.speed;
    xfer[1].bits_per_word = _config.bits_per_word;

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(2), xfer);
    if (retv < 0) {
        log::error("[SPI] Error {} from ioctl (read buffer): {}", errno, strerror(errno));
        return error::fail();
    }

    return error::ok();
}

auto spi::write_buffer(const uint16_t addr, const uint8_t* buffer, size_t length) -> error {

    if (!buffer || length == 0) {
        log::error("spi: invalid buffer or length for write op");
        return error::invalid_arg();
    }

    const auto write_reg = htobe16(addr);

    struct spi_ioc_transfer xfer[2];
    memset(xfer, 0x00, sizeof(xfer));

    xfer[0].tx_buf = reinterpret_cast<__u64>(&write_reg);
    xfer[0].rx_buf = 0x00;
    xfer[0].len = sizeof(write_reg);
    xfer[0].speed_hz = _config.speed;
    xfer[0].bits_per_word = _config.bits_per_word;

    xfer[1].tx_buf = reinterpret_cast<__u64>(buffer);
    xfer[1].rx_buf = 0x00;
    xfer[1].len = length;
    xfer[1].speed_hz = _config.speed;
    xfer[1].bits_per_word = _config.bits_per_word;

    int retv = ioctl(_device_fd, SPI_IOC_MESSAGE(2), xfer);
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
