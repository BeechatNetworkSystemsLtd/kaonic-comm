#pragma once

#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::comm {

class rf215_radio final : public radio {

public:
    explicit rf215_radio() = default;
    ~rf215_radio() = default;

    [[nodiscard]] auto open() -> error final;

    [[nodiscard]] auto transmit(const tx_packet& packet, tx_response& response) -> error final;

    [[nodiscard]] auto recieve(const rx_request& packet, rx_packet& response) -> error final;

    [[nodiscard]] auto set_config(const radio_config& config) -> error final;

    auto close() -> void final;

protected:
    rf215_radio(const rf215_radio&) = delete;
    rf215_radio(rf215_radio&&) = delete;

    rf215_radio& operator=(const rf215_radio&) = delete;
    rf215_radio& operator=(rf215_radio&&) = delete;

private:
    radio_config _config;
};

} // namespace kaonic::comm
