#include "kaonic/comm/peripheral/usb_peripheral.hpp"

#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "kaonic/comm/protocols/hdlc.hpp"

namespace kaonic::peripheral {

constexpr static size_t buffer_size = 256;

constexpr static uint8_t FRAME_FLAG = 0x7E;

static uint8_t read_buffer[buffer_size] = { 0 };

auto usb_peripheral::open(const usb_config& config) -> error {
    _comport_fd = ::open(config.comport_path.c_str(), O_RDWR);

    struct termios tty;
    if (tcgetattr(_comport_fd, &tty) != 0) {
        log::error("[Usb peripheral] Error {} from tcgetattr: {}", errno, strerror(errno));
        return error::fail();
    }

    tty.c_cflag &= ~PARENB;        // Disabling parity.
    tty.c_cflag &= ~CSTOPB;        // Clear stop field, only one stop bit.
    tty.c_cflag &= ~CSIZE;         // Clear all bits that set the data size.
    tty.c_cflag |= CS8;            // 8 bits per byte (most common).
    tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control (most common).
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1).

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                   // Disable echo.
    tty.c_lflag &= ~ECHOE;                  // Disable erasure.
    tty.c_lflag &= ~ECHONL;                 // Disable new-line echo.
    tty.c_lflag &= ~ISIG;                   // Disable interpretation of INTR, QUIT and SUSP.
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds)
    tty.c_cc[VMIN] = 0;

    if (config.bound_rate == 115200) {
        cfsetispeed(&tty, B115200);
        cfsetospeed(&tty, B115200);
    } else {
        log::error("[Usb peripheral] Bound rate {} is not supported", config.bound_rate);
        return error::invalid_arg();
    }

    if (tcsetattr(_comport_fd, TCSANOW, &tty) != 0) {
        log::error("[Usb peripheral] Error {} from tcsetattr: {}", errno, strerror(errno));
        return error::fail();
    }

    return error::ok();
}

auto usb_peripheral::read(protocols::hdlc_frame_t& frame) -> error {
    frame.clear();
    bool frame_started = false;

    ssize_t bytes_read = ::read(_comport_fd, &read_buffer, sizeof(read_buffer));

    if (bytes_read == -1) {
        log::error("[Usb peripheral] Problem occured while reading from the serial port");
        return error::fail();
    }

    if (bytes_read == 0) {
        return error::ok();
    }

    frame.reserve(sizeof(read_buffer));
    for (size_t i = 0; i < sizeof(read_buffer); ++i) {
        auto byte = read_buffer[i];

        if (byte == FRAME_FLAG && !frame_started) {
            frame.push_back(byte);
            frame_started = true;
        }

        if (byte != FRAME_FLAG && frame_started) {
            frame.push_back(byte);
        }

        if (byte == FRAME_FLAG && frame_started) {
            frame.push_back(byte);
            break;
        }
    }

    return error::ok();
}

auto usb_peripheral::write(const protocols::hdlc_frame_t& frame) -> error {
    if (::write(_comport_fd, frame.data(), frame.size()) == -1) {
        log::error("[Usb peripheral] Problem occured while writing to the serial port");
        return error::fail();
    }
    return error::ok();
}

auto usb_peripheral::close() noexcept -> void {
    if (::close(_comport_fd) == -1) {
        log::error(
            "[Usb peripheral] Error {} while closing serial port: {}", errno, strerror(errno));
    }
}

} // namespace kaonic::peripheral
