#include "kaonic/comm/services/radio_service.hpp"

#include <chrono>

#include "kaonic/common/logging.hpp"

using namespace std::chrono_literals;

namespace kaonic::comm {

radio_service::radio_service(const mesh::config& config,
                             const std::vector<std::shared_ptr<radio>>& radios) noexcept
    : _radios { radios }
    , _radio_broadcasters { _radios.size(), std::make_shared<mesh::network_broadcast_receiver>() } {

    for (size_t i = 0; i < _radios.size(); ++i) {

        log::debug("radio: create network");

        auto net =
            std::make_shared<mesh::radio_network>(config, _radios[i], _radio_broadcasters[i]);

        _radio_networks.push_back(net);
    }

    for (const auto& net : _radio_networks) {
        log::debug("radio: start network");
        net->start();
    }
}

auto radio_service::configure(uint8_t module, const radio_config& config) -> error {
    if (module >= _radio_networks.size()) {
        log::error("[Radio Service] Unable to configure radio: invalid module index");
        return error::invalid_arg();
    }

    return _radio_networks[module]->configure(config);
}

auto radio_service::transmit(uint8_t module, const mesh::frame& frame) -> error {
    if (module >= _radio_networks.size()) {
        log::error("[Radio Service] Unable to transmit: invalid module index");
        return error::invalid_arg();
    }

    return _radio_networks[module]->transmit(frame);
}

auto radio_service::attach_listener(
    const std::shared_ptr<mesh::network_receiver>& listener) noexcept -> void {
    if (listener) {
        for (const auto& broadcaster : _radio_broadcasters) {
            broadcaster->attach_listener(listener);
        }
    }
}

} // namespace kaonic::comm
