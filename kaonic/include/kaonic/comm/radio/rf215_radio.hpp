#pragma once

#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::comm {

class rf215_radio final : public radio {

public:
    explicit rf215_radio() = default;
    ~rf215_radio();

    [[nodiscard]] auto open(const radio_config& config) -> error final;

    [[nodiscard]] auto transmit(const tx_request& request, tx_response& response) -> error final;

    [[nodiscard]] auto recieve(const rx_request& request, rx_response& response) -> error final;

    [[nodiscard]] auto configure(const radio_config& config) -> error final;

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
