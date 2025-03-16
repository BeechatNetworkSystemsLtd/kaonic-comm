#include "kaonic/comm/radio/rf215_radio.hpp"

#include <chrono>

#include "kaonic/common/logging.hpp"

extern "C" {
#include "rf215/rf215_baseband.h"
}

using namespace std::chrono_literals;

namespace kaonic::comm {

constexpr static bool rf215_log_verbose = false;

rf215_radio::rf215_radio(const rf215_radio_config& config) noexcept
    : _config { config }
    , _spi { std::make_unique<drivers::spi>() }
    , _irq_buffer { std::make_unique<gpiod::edge_event_buffer>(1) } {

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

static auto init_gpio_output(const drivers::gpio_spec& spec,
                             std::string_view gpio_name,
                             bool active_low = false) noexcept
    -> std::unique_ptr<gpiod::line_request> {

    auto req = std::make_unique<gpiod::line_request>(
        gpiod::chip(spec.gpio_chip)
            .prepare_request()
            .set_consumer(std::string { gpio_name })
            .add_line_settings(spec.gpio_line,
                               gpiod::line_settings()
                                   .set_direction(gpiod::line::direction::OUTPUT)
                                   .set_active_low(active_low)
                                   .set_output_value(gpiod::line::value::INACTIVE))
            .do_request());

    if (!(*req)) {
        log::error("rf215: can't request {} gpio output", gpio_name);
        return nullptr;
    }

    return std::move(req);
}

static auto init_gpio_input(const drivers::gpio_spec& spec,
                            std::string_view gpio_name,
                            bool active_low = false) noexcept
    -> std::unique_ptr<gpiod::line_request> {

    auto req = std::make_unique<gpiod::line_request>(
        gpiod::chip(spec.gpio_chip)
            .prepare_request()
            .set_consumer(std::string { gpio_name })
            .add_line_settings(spec.gpio_line,
                               gpiod::line_settings()
                                   .set_direction(gpiod::line::direction::INPUT)
                                   .set_active_low(active_low)
                                   .set_bias(gpiod::line::bias::PULL_DOWN)
                                   .set_edge_detection(gpiod::line::edge::RISING))
            .do_request());

    if (!(*req)) {
        log::error("rf215: can't request {} gpio input", gpio_name);
        return nullptr;
    }

    return std::move(req);
}

auto rf215_radio::init() -> error {

    auto err = error::ok();
    err += _spi->open(_config.spi);

    _irq_gpio_req = init_gpio_input(_config.irq_gpio, _config.name + "-irq");
    if (!_irq_gpio_req) {
        return error::fail();
    }

    _rst_gpio_req = init_gpio_output(_config.rst_gpio, _config.name + "-rst", true);
    if (!_rst_gpio_req) {
        return error::fail();
    }

    _flt_sel_v1_gpio_req = init_gpio_output(_config.flt_sel_v1_gpio, _config.name + "-flt-sel-v1");
    if (!_flt_sel_v1_gpio_req) {
        return error::fail();
    }

    _flt_sel_v2_gpio_req = init_gpio_output(_config.flt_sel_v2_gpio, _config.name + "-flt-sel-v2");
    if (!_flt_sel_v2_gpio_req) {
        return error::fail();
    }

    _flt_sel_24_gpio_req = init_gpio_output(_config.flt_sel_24_gpio, _config.name + "-flt-sel-24");
    if (!_flt_sel_24_gpio_req) {
        return error::fail();
    }

    if (auto err = rf215_init(&_dev); err != 0) {
        log::error("rf215: Unable to init the radio");
        return error::fail();
    }

    const auto rf215_pn = rf215_probe(&_dev);

    log::info("rf215: detected '0x{:08x}' rf215 part-number", static_cast<int>(rf215_pn));

    return error::ok();
}

auto rf215_radio::configure(const radio_config& config) -> error {

    log::debug("rf215: configure to {}kHz {}ch {}kHz spacing",
               config.freq,
               config.channel,
               config.channel_spacing);

    _active_trx = rf215_get_trx_by_freq(&_dev, config.freq);

    // Filter selection
    // TODO: Allow external configuration of filter
    if (902000 <= config.freq && config.freq <= 928000) {

        log::debug("rf215: flt(V1=1;V2=1)");

        _flt_sel_v1_gpio_req->set_value(_config.flt_sel_v1_gpio.gpio_line,
                                        gpiod::line::value::ACTIVE);
        _flt_sel_v2_gpio_req->set_value(_config.flt_sel_v2_gpio.gpio_line,
                                        gpiod::line::value::ACTIVE);
    } else if (862000 <= config.freq && config.freq <= 876000) {

        log::debug("rf215: flt(V1=0;V2=1)");

        _flt_sel_v1_gpio_req->set_value(_config.flt_sel_v1_gpio.gpio_line,
                                        gpiod::line::value::INACTIVE);
        _flt_sel_v2_gpio_req->set_value(_config.flt_sel_v2_gpio.gpio_line,
                                        gpiod::line::value::ACTIVE);
    } else {

        log::debug("rf215: flt(V1=1;V2=0)");

        _flt_sel_v1_gpio_req->set_value(_config.flt_sel_v1_gpio.gpio_line,
                                        gpiod::line::value::ACTIVE);
        _flt_sel_v2_gpio_req->set_value(_config.flt_sel_v2_gpio.gpio_line,
                                        gpiod::line::value::INACTIVE);
    }

    auto rf = &_dev;

    // Configure registers
    const struct rf215_reg_value mod_ofdm_values[] = {
        // clang-format off

        // Radio
        { rf->common_regs->RG_RF_CLKO, 0x02 },

        { rf->rf09.radio_regs->RG_CMD, 0x02 },
        { rf->rf24.radio_regs->RG_IRQM, 0x00 },
        { rf->rf09.radio_regs->RG_IRQM, 0x1F },

        { rf->rf09.radio_regs->RG_RXBWC, 0x1A },
        { rf->rf09.radio_regs->RG_RXDFE, 0x83 },

        { rf->rf09.radio_regs->RG_EDD, 0x7A },

        { rf->rf09.radio_regs->RG_TXCUTC, 0x0A },
        { rf->rf09.radio_regs->RG_TXDFE, 0x83 },

        // 7 6 5   4 3 2 1 0
        // – PACUR TXPWR
        { rf->rf09.radio_regs->RG_PAC, 0b01100000 + 10 },
        
        // 7 6   5 4 3 2 1 0
        // PADFE – – – – – – 
        { rf->rf09.radio_regs->RG_PADFE, 0b10000000 },

        // 7          6 5    4     3    2   1 0
        // EXTLNAB YP AGCMAP AVEXT AVEN AVS PAVC
        { rf->rf09.radio_regs->RG_AUXS, 0b01000010 },

        // Baseband
        { rf->rf24.baseband_regs->RG_IRQM, 0x00 },
        { rf->rf09.baseband_regs->RG_IRQM, 0x12 },
        { rf->rf09.baseband_regs->RG_PC, 0x0E },
        { rf->rf09.baseband_regs->RG_OFDMC, 0x00 },
        { rf->rf09.baseband_regs->RG_OFDMPHRTX, 0x06 },

        // clang-format on
    };

    const struct rf215_reg_set mod_ofdm_reg_set = {
        .values = mod_ofdm_values,
        .len = RF215_REG_VALUE_ARRAY_COUNT(mod_ofdm_values),
    };

    rf215_write_reg_set(rf, &mod_ofdm_reg_set);

    for (size_t i = 0; i < mod_ofdm_reg_set.len; ++i) {
        uint8_t actual_reg = 0;
        rf215_read_reg(rf, mod_ofdm_reg_set.values[i].reg, &actual_reg);
        if (actual_reg != mod_ofdm_reg_set.values[i].value) {
            log::warn("rf215: incorrect reg 0x{:04x} - {:02x} != {:02x}",
                      mod_ofdm_reg_set.values[i].reg,
                      actual_reg,
                      mod_ofdm_reg_set.values[i].value);
        }
    }

    const rf215_freq freq {
        .channel_spacing = config.channel_spacing,
        .frequency = config.freq,
        .channel = config.channel,
    };

    if (auto err = rf215_set_freq(_active_trx, &freq); err != 0) {
        log::error("rf215: set radio frequency failed");
        return error::fail();
    }

    return error::ok();
}

static auto print_frame(const radio_frame& frame, std::string_view name) noexcept {
    printf("<frame>:[%4dB] [%s]\n\r", (int)frame.len, name.data());
    for (size_t i = 0x00; i < frame.len; ++i) {
        printf("%02x ", frame.data[i]);
        if (((i + 1) % 14) == 0) {
            printf("|\n\r");
        }
    }
    printf("<end>\n\r");
}

auto rf215_radio::transmit(const radio_frame& frame) -> error {

    if (!_active_trx) {
        log::error("rf215: trx wasn't configured");
        return error::precondition_failed();
    }

    // log::trace("rf215: transmit {} bytes", frame.len);
    // print_frame(frame, "TX");

    rf215_frame rf_frame { 0 };
    rf_frame.len = frame.len;
    memcpy(rf_frame.data, frame.data, frame.len);

    if (auto rc = rf215_baseband_tx_frame(_active_trx, &rf_frame); rc != 0) {
        log::warn("rf215: transmit fail - {}", rc);
        return error::fail();
    }

    return error::ok();
}

auto rf215_radio::receive(radio_frame& frame, const std::chrono::milliseconds& timeout) -> error {

    if (!_active_trx) {
        log::error("rf215: trx wasn't configured");
        return error::precondition_failed();
    }

    uint16_t len = sizeof(frame.data);

    const int rc = rf215_baseband_rx(
        _active_trx, static_cast<rf215_millis_t>(timeout.count()), frame.data, &len);

    auto err = error::timeout();
    if ((rc == 0) && (len > 0)) {
        frame.len = len;
        // log::trace("rf215: receive {} bytes", len);
        // print_frame(frame, "RX");
        err = error::ok();
    }

    return err;
}

auto rf215_radio::write(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int {
    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);

    auto bytes = reinterpret_cast<uint8_t*>(data);

    if (rf215_log_verbose) {
        uint16_t dump = 0x00;
        if (len >= 1) {
            dump |= bytes[0];
        }

        if (len >= 2) {
            dump |= bytes[1];
        }

        log::trace("rf215: wr reg[0x{:04x}]=0x{:04x}", reg, dump);
    }

    if (auto err = self._spi->write_buffer(reg, bytes, len); !err.is_ok()) {
        log::error("rf215: fail to write spi buffer");
        return -1;
    }
    return 0;
}

auto rf215_radio::read(const void* ctx, rf215_reg_t reg, void* data, size_t len) noexcept -> int {

    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);

    auto bytes = reinterpret_cast<uint8_t*>(data);
    if (auto err = self._spi->read_buffer(reg, bytes, len); !err.is_ok()) {
        log::error("rf215: fail to read spi buffer");
        return -1;
    }

    if (rf215_log_verbose) {
        uint16_t dump = 0x00;
        if (len >= 1) {
            dump |= bytes[0];
        }

        if (len >= 2) {
            dump |= bytes[1];
        }

        log::trace("rf215: rd reg[0x{:04x}]=0x{:04x}", reg, dump);
    }

    return 0;
}

auto rf215_radio::wait_irq(const void* ctx, rf215_millis_t timeout, rf215_irq_data_t* irq) noexcept
    -> bool {

    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);

    const auto has_irq = self._irq_gpio_req->wait_edge_events(std::chrono::milliseconds(timeout));

    if (has_irq) {

        self._irq_gpio_req->read_edge_events(*self._irq_buffer);

        auto irq_data_ptr = reinterpret_cast<uint8_t*>(irq);
        if (auto err = self._spi->read_buffer(0x0000, irq_data_ptr, sizeof(rf215_irq_data_t));
            !err.is_ok()) {
            return false;
        }

        if (rf215_log_verbose) {
            log::trace("rf215: irq 0x{:08x}", *irq);
        }

    } else {
        if (rf215_log_verbose) {
            log::trace("rf215: no irq");
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
    log::debug("rf215: reset");

    auto& self = *reinterpret_cast<const rf215_radio*>(ctx);

    self._rst_gpio_req->set_value(self._config.rst_gpio.gpio_line, gpiod::line::value::ACTIVE);
    std::this_thread::sleep_for(25ms);

    self._rst_gpio_req->set_value(self._config.rst_gpio.gpio_line, gpiod::line::value::INACTIVE);
    std::this_thread::sleep_for(25ms);
}

auto rf215_radio::sleep(const void* ctx, rf215_millis_t delay) noexcept -> void {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

} // namespace kaonic::comm
