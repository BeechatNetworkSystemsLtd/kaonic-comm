#include "kaonic/comm/radio/rf215_radio.hpp"

#include <chrono>

#include "kaonic/common/logging.hpp"

extern "C" {
#include "rf215/rf215_baseband.h"
}

using namespace std::chrono_literals;

namespace kaonic::comm {

constexpr static uint32_t freq_offset = 1500000000u;

rf215_radio::rf215_radio(radio_context&& context) noexcept
    : _context { std::move(context) } {
    _dev.iface = rf215_iface {
        .ctx = this,
        .write = write,
        .read = read,
        .hardware_reset = reset,
        .wait_irq = wait_irq,
        .sleep = sleep,
        .current_time = current_time,
    };
}

auto rf215_radio::init() -> error {
    if (auto err = rf215_init(&_dev); err != 0) {
        log::error("[RF215 Radio] Unable to init the radio");
        return error::fail();
    }
    return error::ok();
}

auto rf215_radio::configure(const radio_config& config) -> error {
    auto active_trx = config.freq <= freq_offset ? rf215_trx_type::RF215_TRX_TYPE_RF09
                                                 : rf215_trx_type::RF215_TRX_TYPE_RF24;

    switch (active_trx) {
        case rf215_trx_type::RF215_TRX_TYPE_RF09:
            _active_trx = &_dev.rf09;
            break;
        case rf215_trx_type::RF215_TRX_TYPE_RF24:
            _active_trx = &_dev.rf24;
            break;
    }

    const rf215_freq freq {
        .channel_spacing = config.channel_spacing,
        .frequency = config.freq,
        .channel = config.channel,
    };

    if (auto err = rf215_set_freq(_active_trx, &freq); err != 0) {
        log::error("[RF215 Radio] Unable to set radio frequency");
        return error::fail();
    }

    return error::ok();
}

auto rf215_radio::transmit(const radio_frame& frame) -> error {
    if (!_active_trx) {
        log::error("[RF215 Radio] Unable to transmit frame: trx wasn't configured");
        return error::precondition_failed();
    }

    rf215_frame rf_frame { 0 };
    rf_frame.len = frame.len;
    memcpy(rf_frame.data, frame.data, frame.len);

    if (auto err = rf215_baseband_tx_frame(_active_trx, &rf_frame); err != 0) {
        log::error("[RF215 Radio] Unable to rf215_baseband_tx_frame");
        return error::fail();
    }

    return error::ok();
}

auto rf215_radio::receive(radio_frame& frame, const std::chrono::milliseconds& timeout) -> error {
    if (!_active_trx) {
        log::error("[RF215 Radio] Unable to receive frame: trx wasn't configured");
        return error::precondition_failed();
    }

    rf215_frame rf_frame { 0 };
    if (auto err = rf215_baseband_rx_frame(
            _active_trx, static_cast<rf215_millis_t>(timeout.count()), &rf_frame);
        err != 0) {
        log::error("[RF215 Radio] Unable to rf215_baseband_rx_frame");
        return error::timeout();
    }

    memcpy(frame.data, rf_frame.data, rf_frame.len);

    return error::ok();
}

auto rf215_radio::write(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int {
    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);
    if (auto err = self._context.spi->write_buffer(reg, reinterpret_cast<uint8_t*>(data), len);
        !err.is_ok()) {
        log::error("[RF215 Radio] Unable to write SPI buffer");
        return -1;
    }
    return 0;
}

auto rf215_radio::read(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int {
    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);
    if (auto err = self._context.spi->read_buffer(reg, reinterpret_cast<uint8_t*>(data), len);
        !err.is_ok()) {
        log::error("[RF215 Radio] Unable to read SPI buffer");
        return -1;
    }
    return 0;
}

auto rf215_radio::wait_irq(const void* ctx, rf215_millis_t timeout, rf215_irq_data_t* irq) noexcept
    -> bool {
    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);

    const auto& context = self._context;

    const auto has_irq = context.handle_line->event_wait(std::chrono::milliseconds(timeout));
    if (has_irq) {
        auto irq_data_ptr = reinterpret_cast<uint8_t*>(irq);
        if (auto err = context.spi->read_buffer(0x0000, irq_data_ptr, 4); !err.is_ok()) {
            log::error("[RF215 Radio] Unable to wait_irq: SPI IQR buffer failed");
            return false;
        }
    }

    return has_irq;
}

auto rf215_radio::current_time(const void* ctx) noexcept -> rf215_millis_t {
    const auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    return static_cast<rf215_millis_t>(current_time.count());
}

auto rf215_radio::reset(const void* ctx) noexcept -> void {
    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);
    self._context.reset_line->set_value(1);
    std::this_thread::sleep_for(25ms);
    self._context.reset_line->set_value(0);
    std::this_thread::sleep_for(25ms);
}

auto rf215_radio::sleep(const void* ctx, rf215_millis_t delay) noexcept -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

} // namespace kaonic::comm
