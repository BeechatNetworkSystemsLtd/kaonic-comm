#include "kaonic/comm/radio/rf215_radio.hpp"

#include <chrono>
#include <type_traits>
#include <variant>

#include "kaonic/common/logging.hpp"

extern "C" {
#include "rf215/rf215_baseband.h"
}

#include "spdlog/stopwatch.h"

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

    if (rf215_pn != 0x00) {
        log::info("rf215: detected '0x{:08x}' rf215 part-number", static_cast<int>(rf215_pn));
    } else {
        log::error("rf215: chip not detected");
        return error::fail();
    }

    return error::ok();
}

auto rf215_radio::select_filter(const radio_config& config) noexcept -> void {
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
}

auto rf215_radio::configure(const radio_config& config) -> error {

    log::debug("rf215: configure to {}kHz {}ch {}kHz spacing",
               config.freq,
               config.channel,
               config.channel_spacing);

    select_filter(config);

    _active_trx = rf215_get_trx_by_freq(&_dev, config.freq);

    const auto trx_type = _active_trx->type;

    auto rf = &_dev;

    const uint8_t tx_power = static_cast<uint8_t>(std::min(config.tx_power, 12u));

    auto err = error::ok();

    err += std::visit(
        [&](auto&& phy_config) {
            using T = std::decay_t<decltype(phy_config)>;

            int rc = 0;

            // Common registers
            {
                rf215_write_reg(rf, rf->common_regs->RG_RF_CLKO, 0x02);
                rf215_write_reg(rf, _active_trx->radio_regs->RG_CMD, 0x02);
                rf215_write_reg(rf, rf->rf24.radio_regs->RG_IRQM, 0x00);
                rf215_write_reg(rf, rf->rf09.radio_regs->RG_IRQM, 0x00);
                rf215_write_reg(rf, _active_trx->radio_regs->RG_IRQM, 0x1F);
                // 7 6 5   4 3 2 1 0
                // – PACUR TXPWR
                rf215_write_reg(rf,
                                _active_trx->radio_regs->RG_PAC,
                                static_cast<uint8_t>(0b01100000 | tx_power));
                // 7 6   5 4 3 2 1 0
                // PADFE – – – – – –
                rf215_write_reg(rf, _active_trx->radio_regs->RG_PADFE, 0b10000000);

                // 7          6 5    4     3    2   1 0
                // EXTLNAB YP AGCMAP AVEXT AVEN AVS PAVC
                rf215_write_reg(rf, _active_trx->radio_regs->RG_AUXS, 0b01000010);

                rf215_write_reg(rf, rf->rf24.baseband_regs->RG_IRQM, 0x00);
                rf215_write_reg(rf, rf->rf09.baseband_regs->RG_IRQM, 0x00);
                rf215_write_reg(rf, _active_trx->baseband_regs->RG_IRQM, 0x12);
            }

            if constexpr (std::is_same_v<T, radio_phy_config_ofdm>) {

                // Configure registers
                const struct rf215_reg_value mod_values[] = {

                    // Radio
                    { _active_trx->radio_regs->RG_RXBWC, 0x1A },
                    { _active_trx->radio_regs->RG_RXDFE, 0x83 },
                    { _active_trx->radio_regs->RG_EDD, 0x7A },
                    { _active_trx->radio_regs->RG_TXCUTC, 0x0A },
                    { _active_trx->radio_regs->RG_TXDFE, 0x83 },

                    // Baseband
                    { _active_trx->baseband_regs->RG_PC, 0x0E },
                    { _active_trx->baseband_regs->RG_OFDMC,
                      static_cast<uint8_t>(std::min(phy_config.opt, 3u)) },
                    { _active_trx->baseband_regs->RG_OFDMPHRTX,
                      static_cast<uint8_t>(std::min(phy_config.mcs, 6u)) },
                };

                const struct rf215_reg_set reg_set = {
                    .values = mod_values,
                    .len = RF215_REG_VALUE_ARRAY_COUNT(mod_values),
                };

                rc = rf215_write_reg_set(rf, &reg_set);
            }

            if constexpr (std::is_same_v<T, radio_phy_config_fsk>) {

                uint8_t txcutc = 0x00;
                uint8_t txdfe = 0x00;
                uint8_t rxdfe = 0x00;
                uint8_t rxbwc = 0x00;

                // Recommended configuration for Symbol Rate
                // 6.10.4 Transmit Operation and Configuration
                // 6.10.5 Receive Operation and Configuration
                switch (phy_config.srate) {
                    case 0:                                     // 50kHz
                        txdfe |= 8;                             // TXDFE.SR
                        rxdfe |= 10;                            // RXDFE.SR
                        txcutc |= (3 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 0 : 0; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 0 : 0; // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0; // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : 0; // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 0 : 1;        // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 1;        // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : (1 << 5); // RXDFE.RCUT
                        }
                        break;
                    case 1:                                     // 100kHz
                        txdfe |= 4;                             // TXDFE.SR
                        rxdfe |= 5;                             // RXDFE.SR
                        txcutc |= (2 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 1 : 3; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 1 : 3; // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0; // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : 0; // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 1 : 4;        // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0;        // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : (1 << 5); // RXDFE.RCUT
                        }
                        break;
                    case 2:                                     // 150kHz
                        txdfe |= 2;                             // TXDFE.SR
                        rxdfe |= 4;                             // RXDFE.SR
                        txcutc |= (2 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 3 : 5; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 3 : 4; // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0; // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : 0; // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 3 : 6;        // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0;        // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : (2 << 5); // RXDFE.RCUT
                        }
                        break;
                    case 3:                                     // 200kHz
                        txdfe |= 2;                             // TXDFE.SR
                        rxdfe |= 4;                             // RXDFE.SR
                        txcutc |= (2 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 4 : 6; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 3 : 5;               // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0b1000;          // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? (1 << 5) : (1 << 5); // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 4 : 6;               // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0;               // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? (1 << 5) : (3 << 5); // RXDFE.RCUT
                        }
                        break;
                    case 4:                                     // 300kHz
                        txdfe |= 1;                             // TXDFE.SR
                        rxdfe |= 2;                             // RXDFE.SR
                        txcutc |= (1 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 6 : 8; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 5 : 6; // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 1 : 0; // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : 0; // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 6 : 7;        // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0;        // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : (1 << 5); // RXDFE.RCUT
                        }
                        break;
                    case 5:                                     // 400kHz
                        txdfe |= 1;                             // TXDFE.SR
                        rxdfe |= 2;                             // RXDFE.SR
                        txcutc |= (1 << 6);                     // TXCUTC.PARAMP
                        txcutc |= phy_config.midx == 0 ? 7 : 9; // TXCUTC.LPFCUT
                        if (trx_type == RF215_TRX_TYPE_RF09) {
                            rxbwc |= phy_config.midx == 0 ? 6 : 8;        // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0b1000;   // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? 0 : (1 << 5); // RXDFE.RCUT
                        } else {
                            rxbwc |= phy_config.midx == 0 ? 7 : 8;               // RXBWC.BW
                            rxbwc |= phy_config.midx == 0 ? 0 : 0b1000;          // RXBWC.IFS
                            rxdfe |= phy_config.midx == 0 ? (1 << 5) : (2 << 5); // RXDFE.RCUT
                        }
                        break;
                }

                txdfe |= phy_config.midx == 0 ? 0 : (4 << 5);

                // Configure registers
                const struct rf215_reg_value mod_values[] = {

                    // Radio
                    { _active_trx->radio_regs->RG_RXBWC, rxbwc },
                    { _active_trx->radio_regs->RG_RXDFE, rxdfe },
                    { _active_trx->radio_regs->RG_EDD, 0x7A },
                    { _active_trx->radio_regs->RG_TXCUTC, txcutc },
                    { _active_trx->radio_regs->RG_TXDFE, txdfe },
                    { _active_trx->radio_regs->RG_AGCC, 0b1000000u | 0b1 },
                    { _active_trx->radio_regs->RG_AGCS, 0b100000u | 0b10111 },

                    // Baseband
                    { _active_trx->baseband_regs->RG_PC, 0x0E },

                    { _active_trx->baseband_regs->RG_FSKC0,
                      static_cast<uint8_t>(
                          ((phy_config.bt & 0b11) << 6) | ((phy_config.midxs & 0b11) << 4)
                          | ((phy_config.midx & 0b111) << 1) | (phy_config.mord & 0b1)) },

                    { _active_trx->baseband_regs->RG_FSKC1,
                      static_cast<uint8_t>(
                          (phy_config.freq_inversion ? 0b10000 : 0) | (phy_config.srate & 0b1111)
                          | static_cast<uint8_t>(phy_config.preamble_length & 0b1100000000)) },

                    { _active_trx->baseband_regs->RG_FSKC2,
                      static_cast<uint8_t>(
                          ((phy_config.pdtm & 1) << 7) | ((phy_config.rxo & 0b11) << 5)
                          | ((phy_config.rxpto & 1) << 4) | ((phy_config.mse & 1) << 3)
                          | (phy_config.preamble_inversion ? 0b100 : 0u)
                          | ((phy_config.fecs & 1) << 1) | (phy_config.fecie ? 1 : 0)) },

                    { _active_trx->baseband_regs->RG_FSKC3,
                      static_cast<uint8_t>(((phy_config.sfdt & 0b1111) << 4)
                                           | (phy_config.pdt & 0b1111)) },

                    { _active_trx->baseband_regs->RG_FSKC4,
                      static_cast<uint8_t>(
                          (phy_config.sftq ? 0b1000000 : 0) | ((phy_config.sfd32 & 0b1) << 5)
                          | (phy_config.rawbit ? 0b10000 : 0) | ((phy_config.csfd1 & 0b11) << 2)
                          | (phy_config.csfd0 & 0b11)) },

                    { _active_trx->baseband_regs->RG_FSKPLL,
                      static_cast<uint8_t>((phy_config.preamble_length & 0b11111111)) },

                    { _active_trx->baseband_regs->RG_FSKSFD0L,
                      static_cast<uint8_t>((phy_config.sfd0 & 0b11111111)) },
                    { _active_trx->baseband_regs->RG_FSKSFD0H,
                      static_cast<uint8_t>(((phy_config.sfd0 >> 8) & 0b11111111)) },
                    { _active_trx->baseband_regs->RG_FSKSFD1L,
                      static_cast<uint8_t>((phy_config.sfd1 & 0b11111111)) },
                    { _active_trx->baseband_regs->RG_FSKSFD1H,
                      static_cast<uint8_t>(((phy_config.sfd1 >> 8) & 0b11111111)) },

                    { _active_trx->baseband_regs->RG_FSKPHRTX,
                      static_cast<uint8_t>(((phy_config.sfd & 0b1) << 3)
                                           | ((phy_config.dw & 0b1) << 2)) },

                    { _active_trx->baseband_regs->RG_FSKDM,
                      static_cast<uint8_t>((phy_config.pe ? 0b10 : 0)
                                           | (phy_config.en ? 0b01 : 0)) },

                    { _active_trx->baseband_regs->RG_FSKPE0, phy_config.fskpe0 },
                    { _active_trx->baseband_regs->RG_FSKPE1, phy_config.fskpe1 },
                    { _active_trx->baseband_regs->RG_FSKPE2, phy_config.fskpe2 },
                };

                const struct rf215_reg_set reg_set = {
                    .values = mod_values,
                    .len = RF215_REG_VALUE_ARRAY_COUNT(mod_values),
                };

                rc = rf215_write_reg_set(rf, &reg_set);
            }

            return error::from_rc(rc);
        },
        config.phy_config);

    const rf215_freq freq {
        .channel_spacing = config.channel_spacing,
        .frequency = config.freq,
        .channel = config.channel,
    };

    err += error::from_rc(rf215_set_freq(_active_trx, &freq));
    if (!err.is_ok()) {
        log::error("rf215: set radio config failed");
    }

    return err;
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
