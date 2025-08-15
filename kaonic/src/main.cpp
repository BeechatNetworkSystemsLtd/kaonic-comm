#include "version.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "kaonic/common/logging.hpp"

#include "kaonic/comm/mesh/radio_network.hpp"
#include "kaonic/comm/radio/rf215_radio.hpp"
#include "kaonic/comm/serial/serial.hpp"

#include "kaonic/comm/services/grpc_service.hpp"
#include "kaonic/comm/services/radio_service.hpp"

#include <grpcpp/server_builder.h>

using namespace kaonic;
using namespace std::chrono_literals;

// TODO: read config from external file

struct kaonic_machine_config {
    kaonic::comm::rf215_radio_config rfa_config;
    kaonic::comm::rf215_radio_config rfb_config;
};

static constexpr auto rf215_spi_freq = 5 * 1000 * 1000;

static const kaonic_machine_config machine_config_protoa = {
    .rfa_config =
        kaonic::comm::rf215_radio_config {
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
        },
    .rfb_config =
        kaonic::comm::rf215_radio_config {
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
        },
};

static const kaonic_machine_config machine_config_protob = {
    .rfa_config =
        kaonic::comm::rf215_radio_config {
            .name = "rfa",
            .spi =
                (drivers::spi_config) {
                    .dev = "/dev/spidev6.0",
                    .speed = rf215_spi_freq,
                },
            .rst_gpio = { "/dev/gpiochip3", 8 },
            .irq_gpio = { "/dev/gpiochip3", 9 },
            .flt_sel_v1_gpio = { "/dev/gpiochip9", 10 },
            .flt_sel_v2_gpio = { "/dev/gpiochip9", 11 },
            .flt_sel_24_gpio = { "/dev/gpiochip9", 12 },
        },
    .rfb_config =
        kaonic::comm::rf215_radio_config {
            .name = "rfb",
            .spi =
                (drivers::spi_config) {
                    .dev = "/dev/spidev3.0",
                    .speed = 5 * 1000 * 1000,
                },
            .rst_gpio = { "/dev/gpiochip4", 13 },
            .irq_gpio = { "/dev/gpiochip4", 15 },
            .flt_sel_v1_gpio = { "/dev/gpiochip9", 0 },
            .flt_sel_v2_gpio = { "/dev/gpiochip9", 1 },
            .flt_sel_24_gpio = { "/dev/gpiochip9", 2 },
        },
};

static const kaonic_machine_config machine_config_protoc = machine_config_protob;

static auto select_machine_config() noexcept -> const kaonic_machine_config& {

    std::string machine;
    {
        std::ifstream file("/etc/kaonic/kaonic_machine");
        if (file) {
            std::getline(file, machine);
        }
    }

    if (machine == "stm32mp1-kaonic-protoa") {
        return machine_config_protoa;
    }

    if (machine == "stm32mp1-kaonic-protob") {
        return machine_config_protob;
    }

    if (machine == "stm32mp1-kaonic-protoc") {
        return machine_config_protoc;
    }

    return machine_config_protoc;
}

static auto create_radio(const kaonic::comm::rf215_radio_config& config, uint8_t channel)
    -> std::shared_ptr<comm::rf215_radio> {
    auto radio = std::make_shared<comm::rf215_radio>(config);

    if (auto err = radio->init(); !err.is_ok()) {
        log::error("radio init failed");
        return nullptr;
    }

    if (auto err = radio->configure({
            .freq = 869535,
            .channel = channel,
            .channel_spacing = 200,
            .tx_power = 10,
            .phy_config =
                comm::radio_phy_config_ofdm {
                    .mcs = 6,
                    .opt = 0,
                },
        });
        !err.is_ok()) {
        log::error("commd: configuration err");
        return nullptr;
    }

    return radio;
}

auto main(int argc, char** argv) noexcept -> int {

    log::set_level(log::level::trace);

    log::info("commd: start service - {}", kaonic::info::version);

    const auto& machine_config = select_machine_config();

    std::vector<std::shared_ptr<comm::radio>> radios;

    // Initialize Radio Frontend A
    {
        const auto radio = create_radio(machine_config.rfa_config, 11);
        if (radio) {
            radios.push_back(radio);
        }
    }

    // Initialize Radio Frontend B
    if (false) {
        const auto radio = create_radio(machine_config.rfb_config, 1);
        if (radio) {
            radios.push_back(radio);
        }
    }

    const comm::mesh::config mesh_config {
        .packet_pattern = 0xB1EE,
        .slot_duration = 15ms,
        .gap_duration = 2ms,
        .beacon_interval = 500ms,
    };

    const auto radio_service = std::make_shared<comm::radio_service>(mesh_config, radios);

    const auto grpc_service =
        std::make_shared<comm::grpc_service>(radio_service, kaonic::info::version);

    const auto grpc_listener = std::make_shared<comm::grpc_radio_listener>(grpc_service);

    radio_service->attach_listener(grpc_listener);

    log::info("commd: start grpc service");

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(std::string("0.0.0.0:8080"), ::grpc::InsecureServerCredentials());
    builder.RegisterService(grpc_service.get());

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());

    server->Wait();

    log::info("commd: exit");

    return 0;
}
