#include <string>

#include <device.grpc.pb.h>
#include <radio.grpc.pb.h>

#include "kaonic/comm/common/logging.hpp"

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>

const static std::string server_ip { "localhost:50051" };

using namespace kaonic;

auto main(int argc, char** argv) noexcept -> int {
    auto grpc_channel = grpc::CreateChannel(server_ip, grpc::InsecureChannelCredentials());
    auto device_stub = Device::NewStub(grpc_channel);
    auto radio_stub = Radio::NewStub(grpc_channel);
    log::info("[Client] Client started on {} IP", server_ip);

    InfoResponse info {};
    grpc::ClientContext info_context;
    if (auto err = device_stub->GetInfo(&info_context, Empty(), &info); !err.ok()) {
        log::info("[Client] Unable to get info: {}", err.error_message());
        return -1;
    }

    return 0;
}
