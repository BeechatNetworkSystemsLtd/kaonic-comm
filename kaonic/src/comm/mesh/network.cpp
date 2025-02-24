#include "kaonic/comm/mesh/network.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::mesh {

network::network(const config& config, const context& context) noexcept
    : _context { context } {
    if (!_context.net_interface) {
        log::error("[Network Mesh] net_interface wasn't initialized");
        return;
    }

    if (!_context.receiver) {
        log::error("[Network Mesh] receiver wasn't initialized");
        return;
    }

    const rfnet_config rf_config {
        .packet_pattern = config.packet_pattern,
        .iface = {
            .ctx = this,
            .tx = tx,
            .rx = rx,
            .gen_id = gen_id,
            .time = time,
            .on_send = on_send,
            .on_receive = on_receive,
        },
        .peer_storage = {
            .peers = peers,
            .count = 0,
        },
        .beacon_interval = static_cast<uint16_t>(config.beacon_interval.count()),
    };
    rfnet_init(&_rfnet, &rf_config);
}

network::~network() {
    rfnet_reset(&_rfnet);
}

auto network::update() noexcept -> void {
    std::lock_guard lock { _mut };

    rfnet_update(&_rfnet);
}

auto network::transmit(const frame& frame) noexcept -> error {
    std::lock_guard lock { _mut };

    if (!rfnet_is_tx_free(&_rfnet)) {
        log::error("[Network Mesh] Unable to transmit: tx is busy");
        return error::not_ready();
    }

    if (auto rc = rfnet_send(&_rfnet, frame.buffer.data(), frame.buffer.size()); rc != 0) {
        log::error("[Network Mesh] Unable to transmit: rfnet_send failed");
        return error::fail();
    }

    return error::ok();
}

auto network::get_stats() noexcept -> stats {
    std::lock_guard lock { _mut };

    rfnet_stats stat;

    rfnet_get_stats(&_rfnet, &stat);

    return stats {
        .tx_speed = stat.tx_speed,
        .rx_speed = stat.rx_speed,
        .rx_counter = stat.rx_counter,
        .tx_counter = stat.tx_counter,
    };
}

auto network::generate_id() noexcept -> uint64_t {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution(0, UINT64_MAX);
    return distribution(generator);
}

auto network::tx(void* ctx, void* data, size_t len) noexcept -> int {
    auto& self = *reinterpret_cast<network*>(ctx);

    auto data_ptr = reinterpret_cast<uint8_t*>(data);

    self.net_frame.buffer.resize(len);
    std::copy(data_ptr, data_ptr + len, self.net_frame.buffer.begin());

    if (auto err = self._context.net_interface->transmit(self.net_frame); !err.is_ok()) {
        log::error("[Network Mesh] Unable to tx");
        return -1;
    }

    return 0;
}

auto network::rx(void* ctx, void* data, size_t max_len) noexcept -> int {
    auto& self = *reinterpret_cast<network*>(ctx);

    auto data_ptr = reinterpret_cast<uint8_t*>(data);

    if (auto err = self._context.net_interface->receive(self.net_frame); !err.is_ok()) {
        return -1;
    }

    if (self.net_frame.buffer.size() > max_len) {
        return -1;
    }

    std::copy(self.net_frame.buffer.begin(), self.net_frame.buffer.end(), data_ptr);

    return 0;
}

auto network::time(void* ctx) noexcept -> rfnet_time_t {
    return static_cast<rfnet_time_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count());
}

auto network::gen_id(void* ctx, rfnet_node_id_t* id) noexcept -> void {
    if (id) {
        *id = generate_id();
    }
}

auto network::on_send(void* ctx) noexcept -> void {}

auto network::on_receive(void* ctx, const void* data, size_t len) noexcept -> void {
    auto& self = *reinterpret_cast<network*>(ctx);

    auto data_ptr = reinterpret_cast<const uint8_t*>(data);

    self.net_frame.buffer.resize(len);
    std::copy(data_ptr, data_ptr + len, self.net_frame.buffer.begin());

    if (self._context.receiver) {
        self._context.receiver->on_receive(self.net_frame);
    }
}

} // namespace kaonic::comm::mesh
