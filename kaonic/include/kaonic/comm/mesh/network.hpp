#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stddef.h>
#include <vector>

extern "C" {
#include "rfnet/rfnet.h"
}

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/comm/mesh/network_receiver.hpp"
#include "kaonic/comm/radio/radio.hpp"
#include "kaonic/common/error.hpp"

namespace kaonic::comm::mesh {

constexpr static size_t peers_storage_size = 16;

struct config final {
    uint16_t packet_pattern;
    uint64_t id_base = 0x00;
    std::chrono::milliseconds slot_duration;
    std::chrono::milliseconds gap_duration;
    std::chrono::milliseconds beacon_interval;
};

struct context final {
    std::shared_ptr<network_interface> net_interface;
    std::shared_ptr<network_receiver> receiver;
};

struct stats final {
    size_t tx_speed;
    size_t rx_speed;

    size_t rx_counter;
    size_t tx_counter;
};

class network final {

public:
    explicit network(const config& config, const context& context) noexcept;
    ~network();

    auto update() noexcept -> void;

    [[nodiscard]] auto transmit(const frame& frame) noexcept -> error;

    [[nodiscard]] auto get_stats() noexcept -> stats;

private:
    [[nodiscard]] static auto generate_id() noexcept -> uint64_t;

    [[nodiscard]] static auto tx(void* ctx, void* data, size_t len) noexcept -> int;

    [[nodiscard]] static auto rx(void* ctx, void* data, size_t max_len) noexcept -> int;

    [[nodiscard]] static auto time(void* ctx) noexcept -> rfnet_time_t;

    static auto gen_id(void* ctx, rfnet_node_id_t* id) noexcept -> void;

    static auto on_send(void* ctx) noexcept -> void;

    static auto on_receive(void* ctx, const void* data, size_t len) noexcept -> void;

protected:
    network(const network&) = default;
    network(network&&) = default;

    network& operator=(const network&) = default;
    network& operator=(network&&) = default;

private:
    config _config;
    context _context;

    rfnet_peer peers[peers_storage_size];

    rfnet _rfnet;

    frame net_frame {};

    mutable std::mutex _mut;
};

} // namespace kaonic::comm::mesh
