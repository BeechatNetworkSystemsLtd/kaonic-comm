#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "kaonic/comm/mesh/network.hpp"
#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/comm/mesh/network_receiver.hpp"
#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::comm::mesh {

class radio_network_interface final : public network_interface {

public:
    explicit radio_network_interface(const std::shared_ptr<radio>& radio) noexcept;
    ~radio_network_interface() final = default;

    [[nodiscard]] auto transmit(const frame& frame) -> error final;

    [[nodiscard]] auto receive(frame& frame) -> error final;

protected:
    radio_network_interface(const radio_network_interface&) = delete;
    radio_network_interface(radio_network_interface&&) = delete;

    radio_network_interface& operator=(const radio_network_interface&) = delete;
    radio_network_interface& operator=(radio_network_interface&&) = delete;

private:
    const std::shared_ptr<radio> _radio;

    radio_frame _tx_frame;
    radio_frame _rx_frame;
};

class radio_network final {

public:
    explicit radio_network(const config& config,
                           const std::shared_ptr<radio>& radio,
                           const std::shared_ptr<network_receiver>& receiver) noexcept;

    radio_network(const radio_network&) = delete;
    radio_network(radio_network&&) = delete;

    ~radio_network() = default;

    [[nodiscard]] auto start() -> error;

    [[nodiscard]] auto stop() -> error;

    [[nodiscard]] auto configure(const radio_config& config) -> error;

    [[nodiscard]] auto transmit(const frame& frame) -> error;

    radio_network& operator=(const radio_network&) = delete;
    radio_network& operator=(radio_network&&) = delete;

private:
    auto update() noexcept -> void;

private:
    std::shared_ptr<radio> _radio;
    std::shared_ptr<network_interface> _network_interface;
    std::shared_ptr<network_receiver> _network_receiver;

    network _network_mesh;

    std::thread _update_thread;

    std::atomic_bool _running { false };

    mutable std::mutex _mut;
};

} // namespace kaonic::comm::mesh
