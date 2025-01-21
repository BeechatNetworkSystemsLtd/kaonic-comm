#include "kaonic/comm/mesh/network_receiver.hpp"

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::mesh {

auto network_broadcast_receiver::attach_listener(
    const std::shared_ptr<network_receiver>& listener) noexcept -> void {
    if (listener) {
        _listeners.push_back(listener);
    }
}

auto network_broadcast_receiver::on_receive(const frame& frame) -> void {
    for (auto& listener_ptr : _listeners) {
        auto listener = listener_ptr.lock();

        if (!listener) {
            log::error("[Radio Broadcaster] Listener is not initialized");
            continue;
        }

        listener->on_receive(frame);
    }
}

} // namespace kaonic::comm::mesh
