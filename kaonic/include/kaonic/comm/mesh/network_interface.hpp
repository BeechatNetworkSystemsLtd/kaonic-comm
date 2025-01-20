#pragma once

#include <cstdint>
#include <stddef.h>
#include <vector>

#include "kaonic/common/error.hpp"

namespace kaonic::comm::mesh {

struct frame final {
    std::vector<uint8_t> buffer;
};

class network_interface {

public:
    virtual ~network_interface() = default;

    [[nodiscard]] virtual auto transmit(const frame& frame) -> error = 0;

    [[nodiscard]] virtual auto receive(frame& frame) -> error = 0;

protected:
    explicit network_interface() = default;

    network_interface(const network_interface&) = default;
    network_interface(network_interface&&) = default;

    network_interface& operator=(const network_interface&) = default;
    network_interface& operator=(network_interface&&) = default;
};

} // namespace kaonic::comm::mesh
