#include "kaonic/comm/radio/rf215_radio.hpp"

namespace kaonic::comm {

auto rf215_radio::open() -> error {
    return error::ok();
}

auto rf215_radio::transmit(const tx_packet& packet, tx_response& response) -> error {
    return error::ok();
}

auto rf215_radio::recieve(const rx_request& request, rx_packet& packet) -> error {
    return error::ok();
}

auto rf215_radio::set_config(const radio_config& config) -> error {
    return error::ok();
}

auto rf215_radio::close() -> void {}

} // namespace kaonic::comm
