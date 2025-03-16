#include "kaonic/comm/mesh/network.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::mesh {

using namespace std::chrono_literals;

constexpr auto nvmem_path = "/sys/bus/nvmem/devices/stm32-romem0/nvmem";

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
    std::unique_lock lock { _mut };

    while (rfnet_is_tx_free(&_rfnet) != 0) {
        lock.unlock();
        std::this_thread::sleep_for(50ms);
        lock.lock();
    }

    if (auto rc = rfnet_send(&_rfnet, frame.buffer.data(), frame.buffer.size()); rc != 0) {
        log::error("net: tx not ready");
        return error::not_ready();
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

    std::ifstream uid_file(nvmem_path, std::ios::binary);
    if (!uid_file) {
        log::warn("net: can't open otp file - generate id");
        std::random_device rd;
        std::mt19937_64 generator(rd());
        std::uniform_int_distribution<uint64_t> distribution(0, UINT64_MAX);
        return distribution(generator);
    }

    uid_file.seekg(13 * sizeof(uint32_t), std::ios::beg);

    // Read 12 bytes (96-bit UID)
    std::vector<unsigned char> uid_data(8);
    uid_file.read(reinterpret_cast<char*>(uid_data.data()), uid_data.size());

    uint64_t uid = 0;
    for (auto byte : uid_data) {
        uid = (uid << 8) | byte;
    }

    return uid;
}

auto network::tx(void* ctx, void* data, size_t len) noexcept -> int {
    auto& self = *reinterpret_cast<network*>(ctx);

    auto data_ptr = reinterpret_cast<uint8_t*>(data);

    self.net_frame.buffer.resize(len);

    std::copy(data_ptr, data_ptr + len, self.net_frame.buffer.begin());

    if (auto err = self._context.net_interface->transmit(self.net_frame); !err.is_ok()) {
        log::error("net: transmit failed");
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

    return self.net_frame.buffer.size();
}

auto network::time(void* ctx) noexcept -> rfnet_time_t {
    // struct timespec ts;
    // clock_gettime(CLOCK_MONOTONIC, &ts);
    // return static_cast<rfnet_time_t>(ts.tv_sec * 1000LLU + ts.tv_nsec / 1000000LLU);
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
