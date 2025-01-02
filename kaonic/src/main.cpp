#include "version.hpp"

#include <memory>

#include "kaonic/comm/common/logging.hpp"
#include "kaonic/comm/radio/rf215_radio.hpp"
#include "kaonic/grpc/context.hpp"
#include "kaonic/grpc/device_service.hpp"
#include "kaonic/grpc/radio_service.hpp"

#include "version.hpp"

#include <grpcpp/server_builder.h>

using namespace kaonic;

auto main(int argc, char** argv) noexcept -> int {
    log::info("[Server] Start comm server with FW version {}", VERSION);

    std::unique_ptr<comm::radio> radio = std::make_unique<comm::rf215_radio>();
    if (auto err = radio->open(comm::radio_config {}); !err.is_ok()) {
        log::error("[Server] Unable to open radio device: {}", err.to_str());
        return -1;
    }

    auto context = std::make_shared<kaonic::grpc::context>(std::move(radio), VERSION);
    kaonic::grpc::device_service device_svc { context };
    kaonic::grpc::radio_service radio_svc { context };

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", ::grpc::InsecureServerCredentials());
    builder.RegisterService(&device_svc);
    builder.RegisterService(&radio_svc);

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    log::info("[Server] Server is listening on port 50051");

    server->Wait();

    return 0;
}
