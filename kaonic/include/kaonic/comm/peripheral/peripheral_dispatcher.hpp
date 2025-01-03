#pragma once

#include <memory>
#include <vector>

#include "kaonic/comm/peripheral/peripheral.hpp"

namespace kaonic::peripheral {

class peripheral_dispatcher final {

public:
    explicit peripheral_dispatcher() noexcept = default;
    ~peripheral_dispatcher();

    [[nodiscard]] auto attach(std::unique_ptr<peripheral>&& peripheral) -> error;

    [[nodiscard]] auto broadcast(const protocols::hdlc_payload_t& payload) -> error;
    [[nodiscard]] auto listen(protocols::hdlc_payload_t& payload) -> error;

protected:
    peripheral_dispatcher(const peripheral_dispatcher&) = delete;
    peripheral_dispatcher(peripheral_dispatcher&&) = delete;

    peripheral_dispatcher& operator=(const peripheral_dispatcher&) = delete;
    peripheral_dispatcher& operator=(peripheral_dispatcher&&) = delete;

private:
    std::vector<std::unique_ptr<peripheral>> _peripherals;
};

} // namespace kaonic::peripheral
