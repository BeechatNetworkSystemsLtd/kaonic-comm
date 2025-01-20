#pragma once

#include <variant>

#include <radio.grpc.pb.h>

namespace kaonic::comm::serial {

struct error_payload final {};

using buffer_t = std::vector<uint8_t>;
using payload_t =
    std::variant<TransmitRequest, ReceiveResponse, ConfigurationRequest, error_payload>;

class packet {

public:
    static auto decode(const buffer_t& buffer, payload_t& payload) noexcept -> void;

    static auto encode(const payload_t& payload, buffer_t& buffer) noexcept -> void;
};

} // namespace kaonic::comm::serial
