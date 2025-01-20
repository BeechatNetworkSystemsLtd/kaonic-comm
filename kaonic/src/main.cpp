#include "version.hpp"

#include <memory>

#include "kaonic/comm/drivers/spi.hpp"
#include "kaonic/comm/mesh/radio_network.hpp"
#include "kaonic/comm/radio/rf215_radio.hpp"
#include "kaonic/comm/serial/serial.hpp"
#include "kaonic/comm/services/grpc_service.hpp"
#include "kaonic/comm/services/radio_service.hpp"
#include "kaonic/comm/services/serial_service.hpp"
#include "kaonic/common/logging.hpp"

#include "config.hpp"
#include "version.hpp"

#include <grpcpp/server_builder.h>

using namespace kaonic;

auto main(int argc, char** argv) noexcept -> int {
    log::info("[Server] Start comm server with FW version {}", kaonic::info::version);

    drivers::spi_config spi_a_config = {
        .device_path = std::string { kaonic::config::rfa_spi.device_path },
        .speed = kaonic::config::rfa_spi.speed,
        .mode = kaonic::config::rfa_spi.mode,
        .bits_per_word = kaonic::config::rfa_spi.bpw,
    };

    drivers::spi_config spi_b_config = {
        .device_path = std::string { kaonic::config::rfb_spi.device_path },
        .speed = kaonic::config::rfb_spi.speed,
        .mode = kaonic::config::rfb_spi.mode,
        .bits_per_word = kaonic::config::rfb_spi.bpw,
    };

    auto spi_a = std::make_unique<drivers::spi>();
    if (auto err = spi_a->open(spi_a_config); !err.is_ok()) {
        log::info("[Server] Unable to open SPI A driver");
        return -1;
    }

    auto spi_b = std::make_unique<drivers::spi>();
    if (auto err = spi_b->open(spi_b_config); !err.is_ok()) {
        log::info("[Server] Unable to open SPI B driver");
        return -1;
    }

    comm::radio_context radio_context_a;
    radio_context_a.spi = std::move(spi_a);

    comm::radio_context radio_context_b;
    radio_context_b.spi = std::move(spi_b);

    auto gpio_a_rst_chip = gpiod::chip(std::string { kaonic::config::rfa_gpio.rst_chip_name },
                                       gpiod::chip::OPEN_BY_NAME);

    auto gpio_b_rst_chip = gpiod::chip(std::string { kaonic::config::rfb_gpio.rst_chip_name },
                                       gpiod::chip::OPEN_BY_NAME);

    auto gpio_a_irq_chip = gpiod::chip(std::string { kaonic::config::rfa_gpio.irq_chip_name },
                                       gpiod::chip::OPEN_BY_NAME);

    auto gpio_b_irq_chip = gpiod::chip(std::string { kaonic::config::rfb_gpio.irq_chip_name },
                                       gpiod::chip::OPEN_BY_NAME);

    auto rst_line_a = gpio_a_rst_chip.get_line(kaonic::config::rfa_gpio.rst_chip_line);
    auto rst_line_b = gpio_b_rst_chip.get_line(kaonic::config::rfb_gpio.rst_chip_line);

    auto irq_line_a = gpio_a_irq_chip.get_line(kaonic::config::rfa_gpio.irq_chip_line);
    auto irq_line_b = gpio_b_irq_chip.get_line(kaonic::config::rfb_gpio.irq_chip_line);

    if (!rst_line_a) {
        log::error("[Server] Unable to get rst line A {}", kaonic::config::rfa_gpio.rst_chip_line);
        return -1;
    }

    if (!rst_line_b) {
        log::error("[Server] Unable to get rst line B {}", kaonic::config::rfb_gpio.rst_chip_line);
        return -1;
    }

    if (!irq_line_a) {
        log::error("[Server] Unable to get IRQ line A {}", kaonic::config::rfa_gpio.irq_chip_line);
        return -1;
    }

    if (!irq_line_b) {
        log::error("[Server] Unable to get IRQ line B {}", kaonic::config::rfb_gpio.irq_chip_line);
        return -1;
    }

    rst_line_a.request({ "kaonic", gpiod::line_request::DIRECTION_OUTPUT, 0 });
    rst_line_b.request({ "kaonic", gpiod::line_request::DIRECTION_OUTPUT, 0 });

    irq_line_a.request({ "kaonic", gpiod::line_request::DIRECTION_INPUT, 0 });
    irq_line_b.request({ "kaonic", gpiod::line_request::DIRECTION_INPUT, 0 });

    if (!rst_line_a.is_requested()) {
        log::error("[Server] Unable to request reset line A on output");
        return -1;
    }

    if (!rst_line_b.is_requested()) {
        log::error("[Server] Unable to request reset line B on output");
        return -1;
    }

    if (!irq_line_a.is_requested()) {
        log::error("[Server] Unable to request IRQ line A on input");
        return -1;
    }

    if (!irq_line_b.is_requested()) {
        log::error("[Server] Unable to request IRQ line B on input");
        return -1;
    }

    radio_context_a.reset_line = std::make_unique<gpiod::line>(std::move(rst_line_a));
    radio_context_b.handle_line = std::make_unique<gpiod::line>(std::move(rst_line_b));

    radio_context_a.reset_line = std::make_unique<gpiod::line>(std::move(irq_line_a));
    radio_context_b.handle_line = std::make_unique<gpiod::line>(std::move(irq_line_b));

    auto radio_a = std::make_shared<comm::rf215_radio>(std::move(radio_context_a));
    if (auto err = radio_a->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 1");
        return -1;
    }

    auto radio_b = std::make_shared<comm::rf215_radio>(std::move(radio_context_b));
    if (auto err = radio_b->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 2");
        return -1;
    }

    std::vector<std::shared_ptr<comm::radio>> radios { radio_a, radio_b };
    auto radio_service = std::make_shared<comm::radio_service>(radios);

    comm::serial::config serial_config {
        .tty_path = std::string { kaonic::config::KAONIC_SERIAL_TTY_PATH },
        .baud_rate = kaonic::config::KAONIC_SERIAL_BAUD_RATE,
    };
    auto serial = std::make_shared<comm::serial::serial>();

    if (auto err = serial->open(serial_config); !err.is_ok()) {
        log::error("[Server] Unable to open serial device");
        return -1;
    }

    auto grpc_service = std::make_shared<comm::grpc_service>(radio_service, kaonic::info::version);
    auto serial_service = std::make_shared<comm::serial_service>(serial, radio_service);

    radio_service->attach_listener(std::make_shared<comm::serial_radio_listener>(serial_service));
    radio_service->attach_listener(std::make_shared<comm::grpc_radio_listener>(grpc_service));

    if (auto err = serial_service->start_tx(); !err.is_ok()) {
        log::error("[Server] Unable to start serial RX monitoring");
        return -1;
    }

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(std::string(kaonic::config::server_url),
                             ::grpc::InsecureServerCredentials());
    builder.RegisterService(grpc_service.get());

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    log::info("[Server] Server is listening on url {}", kaonic::config::server_url);

    server->Wait();

    return 0;
}
