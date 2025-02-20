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
using namespace std::chrono_literals;

static auto create_rf215(const drivers::spi_config& spi_config,
                         const kaonic::config::gpio_config& gpio_config,
                         std::string name) -> std::shared_ptr<comm::rf215_radio> {

    auto spi = std::make_unique<drivers::spi>();
    if (auto err = spi->open(spi_config); !err.is_ok()) {
        log::info("[Server] Unable to open SPI driver {}", spi_config.device_path);
        return nullptr;
    }

    comm::radio_context radio_context;
    radio_context.spi = std::move(spi);
    radio_context.name = name;

    radio_context.rst_gpio_chip = gpio_config.rst_chip_name;
    radio_context.rst_gpio = gpio_config.rst_chip_line;

    radio_context.irq_gpio_chip = gpio_config.irq_chip_name;
    radio_context.irq_gpio = gpio_config.irq_chip_line;

    return std::make_shared<comm::rf215_radio>(std::move(radio_context));
}

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

    auto radio_a = create_rf215(spi_a_config, kaonic::config::rfa_gpio, "rfa");
    auto radio_b = create_rf215(spi_b_config, kaonic::config::rfb_gpio, "rfb");

    if (!radio_a || !radio_a) {
        return -1;
    }

    if (auto err = radio_a->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 1");
        return -1;
    }

    if (auto err = radio_b->init(); !err.is_ok()) {
        log::error("[Server] Unable to init the radio 2");
        return -1;
    }

    std::vector<std::shared_ptr<comm::radio>> radios { radio_a, radio_b };

    const comm::mesh::config mesh_config {
        .packet_pattern = 0x77,
        .slot_duration = 50ms,
        .gap_duration = 5ms,
        .beacon_interval = 100ms,
    };
    auto radio_service = std::make_shared<comm::radio_service>(mesh_config, radios);

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
