#pragma once

#include <memory>

#include "kaonic/comm/mesh/network_interface.hpp"

namespace kaonic::comm::mesh {

class network_receiver {

public:
    virtual ~network_receiver() = default;

    virtual auto on_receive(const frame& frame) -> void = 0;

protected:
    explicit network_receiver() = default;

    network_receiver(const network_receiver&) = default;
    network_receiver(network_receiver&&) = default;

    network_receiver& operator=(const network_receiver&) = default;
    network_receiver& operator=(network_receiver&&) = default;
};

class network_broadcast_receiver final : public network_receiver {

public:
    explicit network_broadcast_receiver() noexcept = default;
    ~network_broadcast_receiver() final = default;

    network_broadcast_receiver(const network_broadcast_receiver&) = default;
    network_broadcast_receiver(network_broadcast_receiver&&) = default;

    auto attach_listener(const std::shared_ptr<network_receiver>& listener) noexcept -> void;

    auto on_receive(const frame& frame) -> void final;

    network_broadcast_receiver& operator=(const network_broadcast_receiver&) = default;
    network_broadcast_receiver& operator=(network_broadcast_receiver&&) = default;

private:
    std::vector<std::weak_ptr<network_receiver>> _listeners;
};

} // namespace kaonic::comm::mesh
