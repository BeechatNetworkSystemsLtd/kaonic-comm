#include "kaonic/comm/services/serial_service.hpp"

#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <zlib.h>

#include "kaonic/common/logging.hpp"

using namespace std::chrono_literals;

namespace kaonic::comm {

constexpr static auto rx_timeout = 100ms;
constexpr static size_t max_hdlc_size = 10240;

static auto buf_pack(const RadioFrame& src, std::vector<uint8_t>& dst) -> void {
    const auto& data = src.data();
    dst.resize(data.size() * sizeof(uint32_t));
    memcpy(dst.data(), data.data(), data.size() * sizeof(uint32_t));
    dst.resize(src.length());
}

static auto buf_unpack(const std::vector<uint8_t>& src, RadioFrame& dst) -> void {
    auto data = dst.mutable_data();

    size_t dst_size = src.size() / sizeof(uint32_t);
    dst_size += (src.size() - dst_size * sizeof(uint32_t)) ? 1 : 0;
    data->Resize(dst_size, 0);
    std::memcpy(data->mutable_data(), src.data(), src.size());

    dst.set_length(src.size());
}

serial_radio_listener::serial_radio_listener(
    const std::shared_ptr<serial_service>& service) noexcept
    : _serial_service { service } {}

auto serial_radio_listener::on_receive(const mesh::frame& frame) -> void {
    _serial_service->receive_frame(frame);
}

serial_service::serial_service(const std::shared_ptr<serial::serial> serial,
                               const std::shared_ptr<radio_service>& service) noexcept
    : _serial { serial }
    , _radio_service { service }
    , _hdlc_processor { max_hdlc_size } {
    if (!_serial) {
        log::error("[Serial Service] Serial wasn't initialized");
        return;
    }

    if (!_radio_service) {
        log::error("[Serial Service] Radio service wasn't initialized");
        return;
    }
}

serial_service::~serial_service() {
    _serial->close();
}

auto serial_service::start_tx() -> error {
    if (_is_active.load()) {
        log::error("[Serial Service] TX monitorring is currently active");
        return error::precondition_failed();
    }

    _is_active.store(true);
    _rx_thread = std::thread(&serial_service::tx, this);

    return error::ok();
}

auto serial_service::tx() -> error {
    uint8_t byte = 0;

    size_t bytes_read = 0;

    while (_is_active) {
        bytes_read = _serial->read(&byte, sizeof(byte), rx_timeout);

        if (bytes_read < 0) {
            log::error("[Serial Service] TX failed: unable to read data");
            continue;
        }

        if (bytes_read = 0) {
            continue;
        }

        if (_hdlc_processor.update(byte, _hdlc_buffer)) {
            uint32_t expected_crc = 0;

            bytes_read = _serial->read(
                reinterpret_cast<uint8_t*>(&expected_crc), sizeof(expected_crc), rx_timeout);

            if (bytes_read != sizeof(expected_crc)) {
                log::warn("[Serial Service] TX failed: CRC missing");
                continue;
            }

            auto actual_crc = 0;
            actual_crc = crc32(actual_crc, _hdlc_buffer.data(), _hdlc_buffer.size());

            if (expected_crc != actual_crc) {
                log::warn("[Serial Service] TX failed: CRC mismatch");
                continue;
            }

            serial::hdlc::unescape(_hdlc_buffer, _unescaped_buffer);
            serial::packet::decode(_unescaped_buffer, _tx_payload);

            handle_packet(_tx_payload);
        }
    }

    return error::ok();
}

auto serial_service::handle_packet(serial::payload_t& payload) noexcept -> void {
    std::visit(
        [this](auto&& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, ConfigurationRequest>) {
                if (auto err = _radio_service->configure(
                        payload.module(),
                        radio_config {
                            .freq = payload.freq(),
                            .channel = static_cast<uint8_t>(payload.channel()),
                            .channel_spacing = payload.channel_spacing(),
                        });
                    !err.is_ok()) {
                    log::warn("[Serial Service] TX failed: unable to configure to the radio");
                    return;
                }
            }
            if constexpr (std::is_same_v<T, TransmitRequest>) {
                buf_pack(payload.frame(), _frame.buffer);
                if (auto err = _radio_service->transmit(payload.module(), _frame); !err.is_ok()) {
                    log::warn("[Serial Service] TX failed: unable to transmit to the radio");
                    return;
                }
            }
            if constexpr (std::is_same_v<T, serial::error_payload>) {
                log::warn("[Serial Service] TX failed: packet type is undefined");
                return;
            }
        },
        payload);
}

auto serial_service::stop_tx() -> error {
    if (!_is_active.load()) {
        log::error("[Serial Service] TX monitorring is not currently active");
        return error::precondition_failed();
    }

    _is_active.store(false);

    if (_rx_thread.joinable()) {
        _rx_thread.join();
    }

    return error::ok();
}

auto serial_service::receive_frame(const mesh::frame& frame) -> void {
    auto& radio_frame = *_rx_response.mutable_frame();
    buf_unpack(frame.buffer, radio_frame);

    serial::packet::encode(_rx_response, _rx_protobuf);
    serial::hdlc::escape(_rx_protobuf, _escaped_buffer);

    if (_serial->write(_rx_protobuf.data(), _rx_protobuf.size()) != _rx_protobuf.size()) {
        log::error("[Serial peripheral] Problem occured while writing to the serial port");
    }
}

} // namespace kaonic::comm
