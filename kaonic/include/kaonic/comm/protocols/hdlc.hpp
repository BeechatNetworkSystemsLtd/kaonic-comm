#pragma once

#include <cstdint>
#include <vector>

namespace kaonic::protocols {

using hdlc_frame_t = std::vector<uint8_t>;
using hdlc_payload_t = std::vector<uint8_t>;

class hdlc final {

public:
    [[nodiscard]] static auto serialize(const hdlc_payload_t& payload) -> hdlc_frame_t;
    [[nodiscard]] static auto deserialize(const hdlc_frame_t& frame) -> hdlc_payload_t;

    [[nodiscard]] static auto validate_frame(const hdlc_frame_t& frame) -> bool;

private:
    [[nodiscard]] static auto calculate_crc(const hdlc_frame_t& frame) -> uint16_t;

    [[nodiscard]] static auto escape_data(const hdlc_payload_t& payload) -> hdlc_payload_t;
    [[nodiscard]] static auto unescape_data(const hdlc_payload_t& payload) -> hdlc_payload_t;
};

} // namespace kaonic::protocols
