#include "version.hpp"

#include <memory>

#include "kaonic/common/logging.hpp"

#include "kaonic/comm/mesh/radio_network.hpp"
#include "kaonic/comm/radio/rf215_radio.hpp"
#include "kaonic/comm/serial/serial.hpp"

#include "kaonic/comm/services/grpc_service.hpp"
#include "kaonic/comm/services/radio_service.hpp"
#include "kaonic/comm/services/serial_service.hpp"

#include <grpcpp/server_builder.h>

using namespace kaonic;
using namespace std::chrono_literals;

// TODO: read config from external file

static constexpr auto rf215_spi_freq = 10 * 1000 * 1000;

static const kaonic::comm::rf215_radio_config rfa_config = {
    .name = "rfa",
    .spi =
        (drivers::spi_config) {
            .dev = "/dev/spidev6.0",
            .speed = rf215_spi_freq,
        },
    .rst_gpio = { "/dev/gpiochip3", 8 },
    .irq_gpio = { "/dev/gpiochip3", 9 },
    .flt_sel_v1_gpio = { "/dev/gpiochip8", 10 },
    .flt_sel_v2_gpio = { "/dev/gpiochip8", 11 },
    .flt_sel_24_gpio = { "/dev/gpiochip8", 12 },
};

static const kaonic::comm::rf215_radio_config rfb_config = {
    .name = "rfb",
    .spi =
        (drivers::spi_config) {
            .dev = "/dev/spidev3.0",
            .speed = rf215_spi_freq,
        },
    .rst_gpio = { "/dev/gpiochip4", 13 },
    .irq_gpio = { "/dev/gpiochip4", 15 },
    .flt_sel_v1_gpio = { "/dev/gpiochip8", 0 },
    .flt_sel_v2_gpio = { "/dev/gpiochip8", 1 },
    .flt_sel_24_gpio = { "/dev/gpiochip8", 2 },
};

static const comm::serial::config serial_config {
    .tty_path = std::string { "/dev/ttyGS0" },
    .baud_rate = 115200,
};

auto main(int argc, char** argv) noexcept -> int {

    log::set_level(log::level::trace);

    log::info("commd: start service - {}", kaonic::info::version);

    const auto radio_a = std::make_shared<comm::rf215_radio>(rfa_config);

    if (auto err = radio_a->init(); !err.is_ok()) {
        log::error("commd: unable to init the radio a");
        return -1;
    }

    if (auto err = radio_a->configure({
            .freq = 869535,
            .channel = 1,
            .channel_spacing = 200,
        });
        !err.is_ok()) {
        log::error("commd: configuration err");
        return -1;
    }

    std::vector<std::shared_ptr<comm::radio>> radios { radio_a };

    const comm::mesh::config mesh_config {
        .packet_pattern = 0xB1EE,
        .slot_duration = 60ms,
        .gap_duration = 10ms,
        .beacon_interval = 500ms,
    };

    const auto radio_service = std::make_shared<comm::radio_service>(mesh_config, radios);

    const auto serial = std::make_shared<comm::serial::serial>();

    if (auto err = serial->open(serial_config); !err.is_ok()) {
        log::error("commd: open serial device failed");
        return -1;
    }

    const auto grpc_service =
        std::make_shared<comm::grpc_service>(radio_service, kaonic::info::version);

    const auto serial_service = std::make_shared<comm::serial_service>(serial, radio_service);

    const auto serial_listener = std::make_shared<comm::serial_radio_listener>(serial_service);
    const auto grpc_listener = std::make_shared<comm::grpc_radio_listener>(grpc_service);

    // radio_service->attach_listener(serial_listener);
    radio_service->attach_listener(grpc_listener);

    log::info("commd: start serial service");

    if (auto err = serial_service->start_tx(); !err.is_ok()) {
        log::error("[Server] Unable to start serial RX monitoring");
        return -1;
    }

    log::info("commd: start grpc service");

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(std::string("0.0.0.0:8080"), ::grpc::InsecureServerCredentials());
    builder.RegisterService(grpc_service.get());

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    server->Wait();

    log::info("commd: exit");

    return 0;
}
