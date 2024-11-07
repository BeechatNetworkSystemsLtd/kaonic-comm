#pragma once

#include "kaonic/grpc/context.hpp"

#include <radio.grpc.pb.h>

#include <grpc/grpc.h>

namespace kaonic::grpc {

class radio_service final : public Radio::Service {

public:
    explicit radio_service(const std::shared_ptr<context>& context);

    [[nodiscard]] auto Configure(::grpc::ServerContext* context,
                                 const ConfigurationRequest* request,
                                 Empty* response) -> ::grpc::Status final;

    [[nodiscard]] auto Transmit(::grpc::ServerContext* context,
                                const TransmitRequest* request,
                                TransmitResponse* response) -> ::grpc::Status final;

    [[nodiscard]] auto Receive(::grpc::ServerContext* context,
                               const ReceiveRequest* request,
                               ReceiveResponse* response) -> ::grpc::Status final;

    [[nodiscard]] auto ReceiveStream(::grpc::ServerContext* context,
                                     const Empty* request,
                                     ::grpc::ServerWriter<RadioFrame>* writer)
        -> ::grpc::Status final;

protected:
    radio_service(const radio_service&) = default;
    radio_service(radio_service&&) = default;

    radio_service& operator=(const radio_service&) = default;
    radio_service& operator=(radio_service&&) = default;

private:
    std::shared_ptr<context> _context = nullptr;
};

} // namespace kaonic::grpc
