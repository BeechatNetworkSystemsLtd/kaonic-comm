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
    if (auto err = radio->open(); !err.is_ok()) {
        log::error("[Server] Unable to open radio device: {}", err.to_str());
        return -1;
    }

    auto context = std::make_shared<kaonic::grpc::context>(std::move(radio), VERSION);
    kaonic::grpc::device_service device_serv(context);
    kaonic::grpc::radio_service radio_serv(context);

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", ::grpc::InsecureServerCredentials());
    builder.RegisterService(&device_serv);
    builder.RegisterService(&radio_serv);

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    log::info("[Server] Server is listening on port 50051");

    server->Wait();

    return 0;
}
