#include "kaonic/grpc/device_service.hpp"

namespace kaonic::grpc {

device_service::device_service(const std::shared_ptr<context>& context)
    : Device::Service {}
    , _context { context } {}

auto grpc::device_service::GetInfo(::grpc::ServerContext* context,
                                   const Empty* request,
                                   InfoResponse* response) -> ::grpc::Status {
    return ::grpc::Status::OK;
}

auto grpc::device_service::GetStatistics(::grpc::ServerContext* context,
                                         const Empty* request,
                                         StatisticsResponse* response) -> ::grpc::Status {
    return ::grpc::Status::OK;
}

} // namespace kaonic::grpc
