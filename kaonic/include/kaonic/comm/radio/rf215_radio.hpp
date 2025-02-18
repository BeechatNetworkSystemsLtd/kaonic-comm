#pragma once

#include <memory>
#include <mutex>

#include <gpiod.hpp>

#include "kaonic/comm/drivers/spi.hpp"
#include "kaonic/comm/mesh/network_interface.hpp"
#include "kaonic/comm/radio/radio.hpp"

extern "C" {
#include "rf215/rf215.h"
}

namespace kaonic::comm {

struct radio_context {
    std::unique_ptr<drivers::spi> spi;
    std::unique_ptr<gpiod::line> reset_line;
    std::unique_ptr<gpiod::line> handle_line;
};

class rf215_radio final : public radio {

public:
    explicit rf215_radio(radio_context&& context) noexcept;
    ~rf215_radio() = default;

    [[nodiscard]] auto init() -> error;

    [[nodiscard]] auto configure(const radio_config& config) -> error final;

    [[nodiscard]] auto transmit(const radio_frame& frame) -> error final;

    [[nodiscard]] auto receive(radio_frame& frame, const std::chrono::milliseconds& timeout)
        -> error final;

private:
    [[nodiscard]] static auto
    write(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int;

    [[nodiscard]] static auto
    read(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int;

    [[nodiscard]] static auto
    wait_irq(const void* ctx, rf215_millis_t timeout, rf215_irq_data_t* irq) noexcept -> bool;

    [[nodiscard]] static auto current_time(const void* ctx) noexcept -> rf215_millis_t;

    static auto reset(const void* ctx) noexcept -> void;

    static auto sleep(const void* ctx, rf215_millis_t delay) noexcept -> void;

protected:
    rf215_radio(const rf215_radio&) = delete;
    rf215_radio(rf215_radio&&) = delete;

    rf215_radio& operator=(const rf215_radio&) = delete;
    rf215_radio& operator=(rf215_radio&&) = delete;

private:
    radio_context _context;

    rf215_device _dev;
    rf215_trx* _active_trx = nullptr;

    mutable std::mutex _mut;
};

} // namespace kaonic::comm
