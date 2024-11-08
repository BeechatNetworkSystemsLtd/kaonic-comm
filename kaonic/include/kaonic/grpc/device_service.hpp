#pragma once

#include "kaonic/grpc/context.hpp"

#include <device.grpc.pb.h>

#include <grpc/grpc.h>

namespace kaonic::grpc {

class device_service final : public Device::Service {

public:
    explicit device_service(const std::shared_ptr<context>& context);
    ~device_service() = default;

    [[nodiscard]] auto GetInfo(::grpc::ServerContext* context,
                               const Empty* request,
                               InfoResponse* response) -> ::grpc::Status final;

    [[nodiscard]] auto GetStatistics(::grpc::ServerContext* context,
                                     const Empty* request,
                                     StatisticsResponse* response) -> ::grpc::Status final;

protected:
    device_service(const device_service&) = delete;
    device_service(device_service&&) noexcept = delete;

    device_service& operator=(const device_service&) = delete;
    device_service& operator=(device_service&&) noexcept = delete;

private:
    std::shared_ptr<context> _context;
};

} // namespace kaonic::grpc
