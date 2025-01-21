#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/comm/serial/hdlc.hpp"
#include "kaonic/comm/serial/packet.hpp"
#include "kaonic/comm/serial/serial.hpp"
#include "kaonic/comm/services/radio_service.hpp"

namespace kaonic::comm {

class serial_service final {

public:
    explicit serial_service(const std::shared_ptr<serial::serial> serial,
                            const std::shared_ptr<radio_service>& service) noexcept;
    ~serial_service();

    serial_service(const serial_service&) = delete;
    serial_service(serial_service&&) = delete;

    [[nodiscard]] auto start_tx() -> error;

    [[nodiscard]] auto stop_tx() -> error;

    auto receive_frame(const mesh::frame& frame) -> void;

    serial_service& operator=(const serial_service&) = delete;
    serial_service& operator=(serial_service&&) = delete;

private:
    [[nodiscard]] auto tx() -> error;

    auto handle_packet(serial::payload_t& payload) noexcept -> void;

private:
    std::shared_ptr<serial::serial> _serial;
    std::shared_ptr<radio_service> _radio_service;

    std::thread _rx_thread;

    std::atomic_bool _is_active { false };

    serial::hdlc_processor _hdlc_processor;

    serial::hdlc_data_t _hdlc_buffer;
    serial::hdlc_data_t _unescaped_buffer;
    serial::hdlc_data_t _escaped_buffer;

    serial::payload_t _tx_payload;

    ReceiveResponse _rx_response;
    std::vector<uint8_t> _rx_protobuf;
    mesh::frame _frame;
};

class serial_radio_listener final : public mesh::network_receiver {

public:
    explicit serial_radio_listener(const std::shared_ptr<serial_service>& service) noexcept;
    ~serial_radio_listener() final = default;

    auto on_receive(const mesh::frame& frame) -> void final;

private:
    std::shared_ptr<serial_service> _serial_service;
};

} // namespace kaonic::comm
