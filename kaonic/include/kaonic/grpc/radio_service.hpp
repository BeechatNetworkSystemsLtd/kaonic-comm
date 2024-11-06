#pragma once

#include <radio.grpc.pb.h>

#include <grpc/grpc.h>

namespace kaonic::grpc {

class radio_service final : public Radio::Service {};

} // namespace kaonic::grpc
