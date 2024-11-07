#include "version.hpp"

#include <memory>

#include "kaonic/grpc/device_service.hpp"
#include "kaonic/grpc/radio_service.hpp"

#include <grpcpp/server_builder.h>

using namespace kaonic;

auto main(int argc, char** argv) noexcept -> int {
    kaonic::grpc::device_service device_serv;
    kaonic::grpc::radio_service radio_serv;

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", ::grpc::InsecureServerCredentials());
    builder.RegisterService(&device_serv);
    builder.RegisterService(&radio_serv);

    std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
    server->Wait();

    return 0;
}
