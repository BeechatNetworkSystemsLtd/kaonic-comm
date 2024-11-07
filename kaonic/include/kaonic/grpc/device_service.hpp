#pragma once

#include <device.grpc.pb.h>

#include <grpc/grpc.h>

namespace kaonic::grpc {

class device_service final : public Device::Service {};

} // namespace kaonic::grpc
