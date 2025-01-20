#include <string>

#include <device.grpc.pb.h>
#include <radio.grpc.pb.h>

#include "kaonic/common/logging.hpp"

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>

const static std::string server_ip { "localhost:50051" };

using namespace kaonic;

auto main(int argc, char** argv) noexcept -> int {
    auto grpc_channel = grpc::CreateChannel(server_ip, grpc::InsecureChannelCredentials());
    auto device_stub = Device::NewStub(grpc_channel);
    auto radio_stub = Radio::NewStub(grpc_channel);
    log::info("[Client] Client started on {} IP", server_ip);

    InfoResponse info;
    grpc::ClientContext info_context;
    if (auto err = device_stub->GetInfo(&info_context, Empty(), &info); !err.ok()) {
        log::info("[Client] Unable to get info: {}", err.error_message());
        return -1;
    }

    grpc::ClientContext recieve_context;
    auto reciever = radio_stub->ReceiveStream(&recieve_context, ReceiveRequest {});
    ReceiveResponse response;
    while (reciever->Read(&response)) {
        const auto& frame = response.frame();
        log::info("[Client] Recieved frame length: {}", frame.length());
        log::info("[Client] Recieved frame buffer elements: ");
        auto bytes_ptr = reinterpret_cast<const uint8_t*>(frame.data().begin());
        for (size_t i = 0; i < frame.length(); ++i) {
            log::info("[Client] {}", bytes_ptr[i]);
        }
    }
    return 0;
}
