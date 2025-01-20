#pragma once

#include <chrono>
#include <cstdint>

#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/common/error.hpp"

constexpr auto data_max_size = 2048u;

namespace kaonic::comm {

struct radio_frame final {
    uint16_t len;
    uint8_t data[data_max_size];
};

struct radio_config final {
    uint32_t freq = 0;
    uint8_t channel = 0;
    uint32_t channel_spacing = 0;
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
