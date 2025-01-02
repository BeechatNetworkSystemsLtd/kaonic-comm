#include "kaonic/comm/radio/rf215_radio.hpp"

namespace kaonic::comm {

rf215_radio::~rf215_radio() {
    close();
}

auto rf215_radio::open(const radio_config& config) -> error {
    return error::ok();
}

auto rf215_radio::transmit(const tx_request& request, tx_response& response) -> error {
    return error::ok();
}

auto rf215_radio::recieve(const rx_request& request, rx_response& response) -> error {
    return error::ok();
}

auto rf215_radio::configure(const radio_config& config) -> error {
    return error::ok();
}

auto rf215_radio::close() -> void {}

} // namespace kaonic::comm
