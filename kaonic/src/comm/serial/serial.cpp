#include "kaonic/comm/serial/serial.hpp"

#include <algorithm>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::serial {

constexpr static std::array<std::pair<uint32_t, uint32_t>, 22> supported_baudrates = { {
    { 110, B110 },         { 150, B150 },         { 300, B300 },         { 1200, B1200 },
    { 2400, B2400 },       { 4800, B4800 },       { 9600, B9600 },       { 19200, B19200 },
    { 38400, B38400 },     { 57600, B57600 },     { 115200, B115200 },   { 230400, B230400 },
    { 460800, B460800 },   { 921600, B921600 },   { 1000000, B1000000 }, { 1152000, B1152000 },
    { 1500000, B1500000 }, { 2000000, B2000000 }, { 2500000, B2500000 }, { 3000000, B3000000 },
    { 3500000, B3500000 }, { 4000000, B4000000 },
} };

auto serial::open(const config& config) -> error {
    if (!std::filesystem::exists(config.tty_path)) {
        log::error("[Serial] Filepath {} doesn`t exist", config.tty_path.string());
        return error::invalid_arg();
    }

    const auto baudrate_itr =
        std::find_if(supported_baudrates.begin(), supported_baudrates.end(), [&](const auto& elem) {
            return config.baud_rate == std::get<0>(elem);
        });

    if (baudrate_itr == supported_baudrates.end()) {
        log::error("[Serial] Baud rate {} is not supported", config.baud_rate);
        return error::invalid_arg();
    }

    std::lock_guard lock { _mut };

    ::close(_comport_fd);
    _comport_fd = ::open(config.tty_path.c_str(), O_RDWR);
    if (_comport_fd == -1) {
        log::error("[Serial] Unable to open file descriptor");
        return error::fail();
    }

    termios uart_options { 0 };
    if (tcgetattr(_comport_fd, &uart_options)) {
        log::error("[Serial] Unable to put the state of FD into termios options ");
        ::close(_comport_fd);
        return error::fail();
    }

    if (auto err = cfsetispeed(&uart_options, baudrate_itr->second); err != 0) {
        log::error("[Serial] Unable to set cfsetispeed parameter: {}", strerror(errno));
        ::close(_comport_fd);
        return error::fail();
    }

    if (auto err = cfsetospeed(&uart_options, baudrate_itr->second); err != 0) {
        log::error("[Serial] Unable to set cfsetospeed parameter: {}", strerror(errno));
        ::close(_comport_fd);
        return error::fail();
    }

    uart_options.c_cflag |= CLOCAL;
    uart_options.c_cflag |= CREAD;

    uart_options.c_cflag &= ~CSIZE;
    uart_options.c_cflag |= CS8;
    uart_options.c_cflag &= ~PARENB;
    uart_options.c_iflag &= ~INPCK;
    uart_options.c_cflag &= ~CSTOPB;

    uart_options.c_cflag &= ~CRTSCTS;
    uart_options.c_iflag &= ~(IXON | IXOFF | IXANY);

    uart_options.c_lflag &= ~ICANON;
    uart_options.c_lflag &= ~ISIG;
    uart_options.c_iflag &= ~(IGNCR | ICRNL);

    uart_options.c_oflag &= ~OPOST;
    uart_options.c_oflag &= ~ONLCR;

    uart_options.c_cc[VTIME] = 10;
    uart_options.c_cc[VMIN] = 0;

    if (auto err = tcflush(_comport_fd, TCIFLUSH); err != 0) {
        log::error("[Serial] Unable to flush pending data on file descriptor: {}", strerror(errno));
        ::close(_comport_fd);
        return error::fail();
    }

    if (auto err = tcsetattr(_comport_fd, TCSANOW, &uart_options); err != 0) {
        log::error("[Serial] Unable to set the state of file descriptor to termios "
                   "options: {}",
                   strerror(errno));
        ::close(_comport_fd);
        return error::fail();
    }

    log::info("[Serial] Open '{}' device, baudrate: {}", config.tty_path.c_str(), config.baud_rate);

    return error::ok();
}

auto serial::read(uint8_t* data, size_t length, std::chrono::milliseconds timeout) -> size_t {
    timeval time;
    time.tv_sec = 0;
    time.tv_usec = static_cast<suseconds_t>(timeout.count()) * 1000;
    size_t bytes_read = 0;

    std::lock_guard lk { _mut };

    fd_set monitor_fd;
    FD_ZERO(&monitor_fd);
    FD_SET(_comport_fd, &monitor_fd);

    auto selected_fd = ::select(_comport_fd + 1, &monitor_fd, nullptr, nullptr, &time);
    if (selected_fd == 0) {
        return 0;
    }

    if (selected_fd < 0) {
        log::error("[Serial] Select error - {}", strerror(errno));
        return -1;
    }

    if (bytes_read = ::read(_comport_fd, data, length); bytes_read < 0) {
        log::error("[Serial] Read error - {}", strerror(errno));
    }

    return bytes_read;
}

auto serial::write(const uint8_t* data, size_t length) -> size_t {
    size_t bytes_writen = 0;

    std::lock_guard lk { _mut };

    if (bytes_writen = ::write(_comport_fd, data, length); bytes_writen < 0) {
        log::error("[Serial] Write error - {}", strerror(errno));
        tcflush(_comport_fd, TCOFLUSH);
    }

    return bytes_writen;
}

auto serial::close() noexcept -> void {
    std::lock_guard lk { _mut };

    ::close(_comport_fd);
}

} // namespace kaonic::comm::serial
