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

    drivers::spi_config spi1_config = {
        .device_path = std::string { kaonic::config::spi1.device_path },
        .speed = kaonic::config::spi1.speed,
        .mode = kaonic::config::spi1.mode,
        .bits_per_word = kaonic::config::spi1.bpw,
    };

    drivers::spi_config spi2_config = {
        .device_path = std::string { kaonic::config::spi2.device_path },
        .speed = kaonic::config::spi2.speed,
        .mode = kaonic::config::spi2.mode,
        .bits_per_word = kaonic::config::spi2.bpw,
    };

    auto spi1 = std::make_unique<drivers::spi>();
    if (auto err = spi1->open(spi1_config); !err.is_ok()) {
        log::info("[Server] Unable to open SPI1 driver");
        return -1;
    }

    auto spi2 = std::make_unique<drivers::spi>();
    if (auto err = spi2->open(spi2_config); !err.is_ok()) {
        log::info("[Server] Unable to open SPI2 driver");
        return -1;
    }

    comm::radio_context radio_context1;
    radio_context1.spi = std::move(spi1);

    comm::radio_context radio_context2;
    radio_context2.spi = std::move(spi2);

    auto gpio_chip1 =
        gpiod::chip(std::string { kaonic::config::gpio1.chip_name }, gpiod::chip::OPEN_BY_NAME);

    auto gpio_chip2 =
        gpiod::chip(std::string { kaonic::config::gpio2.chip_name }, gpiod::chip::OPEN_BY_NAME);

    auto reset_line1 = gpio_chip1.get_line(kaonic::config::gpio1.rst_chip_line);
    auto handle_line1 = gpio_chip1.get_line(kaonic::config::gpio1.iqr_chip_line);

    auto reset_line2 = gpio_chip2.get_line(kaonic::config::gpio2.rst_chip_line);
    auto handle_line2 = gpio_chip2.get_line(kaonic::config::gpio2.iqr_chip_line);

    if (!reset_line1) {
        log::error("[Server] Unable to get rst line 1 {}", kaonic::config::gpio1.rst_chip_line);
        return -1;
    }

    if (!reset_line2) {
        log::error("[Server] Unable to get rst line 2 {}", kaonic::config::gpio2.rst_chip_line);
        return -1;
    }

    if (!handle_line1) {
        log::error("[Server] Unable to get handle line 1 {}", kaonic::config::gpio1.iqr_chip_line);
        return -1;
    }

    if (!handle_line2) {
        log::error("[Server] Unable to get handle line 2 {}", kaonic::config::gpio2.iqr_chip_line);
        return -1;
    }

    reset_line1.request({ "kaonic", gpiod::line_request::DIRECTION_OUTPUT, 0 });
    reset_line2.request({ "kaonic", gpiod::line_request::DIRECTION_OUTPUT, 0 });

    handle_line1.request({ "kaonic", gpiod::line_request::DIRECTION_INPUT, 0 });
    handle_line2.request({ "kaonic", gpiod::line_request::DIRECTION_INPUT, 0 });

    if (!reset_line1.is_requested()) {
        log::error("[Server] Unable to request reset line 1 on output");
        return -1;
    }

    if (!reset_line2.is_requested()) {
        log::error("[Server] Unable to request reset line 2 on output");
        return -1;
    }

    if (!handle_line1.is_requested()) {
        log::error("[Server] Unable to request handle line 1 on input");
        return -1;
    }

    if (!handle_line2.is_requested()) {
        log::error("[Server] Unable to request handle line 2 on input");
        return -1;
    }

    radio_context1.reset_line = std::make_unique<gpiod::line>(std::move(reset_line1));
    radio_context2.handle_line = std::make_unique<gpiod::line>(std::move(handle_line2));

    radio_context1.reset_line = std::make_unique<gpiod::line>(std::move(reset_line1));
    radio_context2.handle_line = std::make_unique<gpiod::line>(std::move(handle_line2));

    auto radio1 = std::make_shared<comm::rf215_radio>(std::move(radio_context1));
    if (auto err = radio1->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 1");
        return -1;
    }

    auto radio2 = std::make_shared<comm::rf215_radio>(std::move(radio_context2));
    if (auto err = radio2->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 2");
        return -1;
    }

    std::vector<std::shared_ptr<comm::radio>> radios { radio1, radio2 };
    auto radio_service = std::make_shared<comm::radio_service>(radios);

    comm::serial::config serial_config {
        .tty_path = std::string { kaonic::config::serial_tty_path },
        .bound_rate = kaonic::config::serial_bound_rate,
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
