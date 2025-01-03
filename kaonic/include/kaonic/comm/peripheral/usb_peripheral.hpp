#pragma once

#include "kaonic/comm/peripheral/peripheral.hpp"
#include "kaonic/common/error.hpp"
#include "kaonic/common/logging.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kaonic::peripheral {

struct usb_config final {
    std::string comport_path;
    uint32_t bound_rate;
};

class usb_peripheral final : public peripheral {

public:
    explicit usb_peripheral() noexcept = default;
    ~usb_peripheral() = default;

    [[nodiscard]] auto open(const usb_config& config) -> error;

    [[nodiscard]] auto read(protocols::hdlc_frame_t& frame) -> error final;

    [[nodiscard]] auto write(const protocols::hdlc_frame_t& frame) -> error final;

    auto close() noexcept -> void final;

protected:
    usb_peripheral(const usb_peripheral&) = delete;
    usb_peripheral(usb_peripheral&&) = delete;

    usb_peripheral& operator=(const usb_peripheral&) = delete;
    usb_peripheral& operator=(usb_peripheral&&) = delete;

private:
    int _comport_fd;
};

} // namespace kaonic::peripheral
