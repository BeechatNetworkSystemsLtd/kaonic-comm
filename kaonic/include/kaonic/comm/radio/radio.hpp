#pragma once

#include <cstdint>

#include "kaonic/comm/common/error.hpp"

constexpr static auto data_max_size = 2048u;

namespace kaonic::comm {

enum class trx_type {
    rf09,
    rf24,
};

struct radio_frame final {
    uint8_t data[data_max_size] = { 0 };
    uint32_t len = 0;
    uint32_t crc = 0;
};

struct radio_config final {
    uint8_t mobule = 0;
    trx_type type = trx_type::rf09;
    uint32_t freq = 0;
    uint32_t channel = 0;
    uint32_t channel_spacing = 0;
};

struct tx_request final {
    uint8_t mobule = 0;
    trx_type type = trx_type::rf09;
    radio_frame frame;
};

struct tx_response final {
    uint32_t latency = 0;
};

struct rx_request final {
    uint8_t module = 0;
    trx_type type = trx_type::rf09;
    uint32_t timeout = 0;
};

struct rx_response final {
    radio_frame frame;
    int8_t rssi = 0;
    uint32_t latency = 0;
};

class radio {

public:
    virtual ~radio() = default;

    virtual auto open(const radio_config& config) -> error = 0;

    virtual auto transmit(const tx_request& request, tx_response& response) -> error = 0;

    virtual auto recieve(const rx_request& request, rx_response& response) -> error = 0;

    virtual auto configure(const radio_config& config) -> error = 0;

    virtual auto close() -> void = 0;

protected:
    explicit radio() = default;

    radio(const radio&) = default;
    radio(radio&&) = default;

    radio& operator=(const radio&) = default;
    radio& operator=(radio&&) = default;
};

} // namespace kaonic::comm
