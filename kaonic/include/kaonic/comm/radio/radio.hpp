#pragma once

#include <chrono>
#include <cstdint>
#include <variant>

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/common/error.hpp"

constexpr auto data_max_size = 2048u;

namespace kaonic::comm {

struct radio_frame final {
    uint16_t len;
    uint8_t data[data_max_size];
};

struct radio_phy_config_ofdm {
    uint32_t mcs = 6;
    uint32_t opt = 0;
};

using radio_phy_config_t = std::variant<radio_phy_config_ofdm>;

struct radio_config final {
    uint32_t freq = 869535;
    uint8_t channel = 1;
    uint32_t channel_spacing = 200;
    uint32_t tx_power = 10;
    radio_phy_config_t phy_config = radio_phy_config_ofdm {};
};

class radio {

public:
    virtual ~radio() = default;

    virtual auto configure(const radio_config& config) -> error = 0;

    virtual auto transmit(const radio_frame& frame) -> error = 0;

    virtual auto receive(radio_frame& frame, const std::chrono::milliseconds& timeout) -> error = 0;

protected:
    explicit radio() = default;

    radio(const radio&) = default;
    radio(radio&&) = default;

    radio& operator=(const radio&) = default;
    radio& operator=(radio&&) = default;
};

} // namespace kaonic::comm
