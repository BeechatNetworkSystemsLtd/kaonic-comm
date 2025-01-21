#pragma once

#include <vector>

#include "kaonic/comm/mesh/radio_network.hpp"
#include "kaonic/comm/radio/radio.hpp"

namespace kaonic::comm {

class radio_service {

public:
    explicit radio_service(const mesh::config& config,
                           const std::vector<std::shared_ptr<radio>>& radios) noexcept;

    [[nodiscard]] auto configure(uint8_t module, const radio_config& config) -> error;

    [[nodiscard]] auto transmit(uint8_t module, const mesh::frame& frame) -> error;

    auto attach_listener(const std::shared_ptr<mesh::network_receiver>& listener) noexcept -> void;

private:
    std::vector<std::shared_ptr<radio>> _radios;
    std::vector<std::shared_ptr<mesh::network_broadcast_receiver>> _radio_broadcasters;
    std::vector<std::shared_ptr<mesh::radio_network>> _radio_networks;
};

} // namespace kaonic::comm
