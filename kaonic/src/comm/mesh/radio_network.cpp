#include "kaonic/comm/mesh/radio_network.hpp"

#include <algorithm>
#include <chrono>
#include <unistd.h>

#include "kaonic/common/logging.hpp"

using namespace std::chrono_literals;

namespace kaonic::comm::mesh {

constexpr static auto rx_timeout = 1ms;

radio_network_interface::radio_network_interface(const std::shared_ptr<radio>& radio) noexcept
    : _radio { radio } {
    if (!_radio) {
        log::error("[Radio Network Interface] Radio wasn't initialized");
    }
}

auto radio_network_interface::transmit(const frame& frame) -> error {
    if (frame.buffer.size() > sizeof(_tx_frame.data)) {
        log::error("[Radio Network Interface] Unable to transmit: max frame size is {}",
                   sizeof(_tx_frame.data));
        return error::invalid_arg();
    }

    std::copy(frame.buffer.begin(), frame.buffer.end(), _tx_frame.data);
    _tx_frame.len = frame.buffer.size();

    if (auto err = _radio->transmit(_tx_frame); !err.is_ok()) {
        log::error("[Radio Network Interface] Unable to transmit radio frame");
        return err;
    }

    return error::ok();
}

auto radio_network_interface::receive(frame& frame) -> error {

    if (auto err = _radio->receive(_rx_frame, rx_timeout); !err.is_ok()) {
        return error::timeout();
    }

    frame.buffer.resize(_rx_frame.len);

    std::copy(_rx_frame.data, _rx_frame.data + _rx_frame.len, frame.buffer.begin());

    return error::ok();
}

radio_network::radio_network(const config& config,
                             const std::shared_ptr<radio>& radio,
                             const std::shared_ptr<network_receiver>& receiver) noexcept
    : _radio { radio }
    , _network_interface { std::make_shared<radio_network_interface>(radio) }
    , _network_receiver { receiver }
    , _network_mesh { config,
                      context {
                          std::make_shared<radio_network_interface>(_radio),
                          _network_receiver,
                      } } {
    if (!_radio) {
        log::error("[Radio Network] Radio wasn't initialized");
        return;
    }

    if (!_network_receiver) {
        log::error("[Radio Network] Network receiver wasn't initialized");
        return;
    }
}

auto radio_network::start() -> error {
    if (_running.load()) {
        return error::precondition_failed();
    }

    _update_thread = std::thread(&radio_network::update, this);
    _running.store(true);

    return error::ok();
}

auto radio_network::stop() -> error {
    if (!_running.load()) {
        return error::precondition_failed();
    }

    _running.store(false);

    if (_update_thread.joinable()) {
        _update_thread.join();
    }

    return error::ok();
}

auto radio_network::configure(const radio_config& config) -> error {
    return _radio->configure(config);
}

auto radio_network::transmit(const frame& frame) -> error {
    return _network_mesh.transmit(frame);
}

auto radio_network::update() noexcept -> void {

    auto report_time = std::chrono::system_clock::now();

    while (_running) {

        _network_mesh.update();

        {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 10000;
            clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
        }
    }
}

} // namespace kaonic::comm::mesh
