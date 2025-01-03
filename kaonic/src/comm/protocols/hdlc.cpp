#include "kaonic/comm/protocols/hdlc.hpp"

#include "kaonic/comm/common/logging.hpp"

#include <stddef.h>

namespace kaonic::protocols {

constexpr static uint8_t FLAG = 0x7E;
constexpr static uint8_t ESCAPE = 0x7D;
constexpr static uint8_t ESCAPE_XOR = 0x20;

auto hdlc::serialize(const hdlc_payload_t& payload) -> hdlc_frame_t {
    uint16_t crc = calculate_crc(payload);

    hdlc_frame_t frame;
    frame.push_back(FLAG);

    hdlc_payload_t data(payload);
    data.push_back(crc >> 8);
    data.push_back(crc & 0xFF);

    hdlc_payload_t escaped_data = escape_data(data);
    frame.insert(frame.end(), escaped_data.begin(), escaped_data.end());
    frame.push_back(FLAG);

    return frame;
}

auto hdlc::deserialize(const hdlc_frame_t& frame) -> hdlc_payload_t {
    if (!validate_frame(frame)) {
        log::error("[HDLC] Invalid frame to deserialize, empty frame returned");
        return hdlc_payload_t {};
    }

    std::vector<uint8_t> data(frame.begin() + 1, frame.end() - 1);
    std::vector<uint8_t> unescaped_data = unescape_data(data);
    if (unescaped_data.size() < 3) {
        log::error("[HDLC] Invalid unescaped data, size is lower then 3, empty frame returned");
        return hdlc_payload_t {};
    }

    uint16_t received_crc = (unescaped_data[unescaped_data.size() - 2] << 8)
        | unescaped_data[unescaped_data.size() - 1];

    std::vector<uint8_t> message(unescaped_data.begin(), unescaped_data.end() - 2);
    uint16_t calculated_crc = calculate_crc(message);
    if (received_crc != calculated_crc) {
        log::error("[HDLC] Invalid frame to deserialize: CRC mismatch. Frame is corrupted");
        return hdlc_payload_t {};
    }

    return message;
}

auto hdlc::validate_frame(const hdlc_frame_t& frame) -> bool {
    if (frame.size() < 5) {
        log::error("[HDLC] Frame validation failed: frame data size is lower than 5");
        return false;
    }

    if (frame.front() != FLAG || frame.back() != FLAG) {
        log::error("[HDLC] Frame validation failed: FLAGs are corrupted");
        return false
    }

    return true;
}

auto hdlc::calculate_crc(const hdlc_frame_t& frame) -> uint16_t {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data) {
        crc ^= (byte << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

auto hdlc::escape_data(const hdlc_payload_t& payload) -> hdlc_payload_t {
    hdlc_payload_t escaped;
    escaped.reserve(data.size());

    for (uint8_t byte : data) {
        if (byte == FLAG || byte == ESCAPE) {
            escaped.push_back(ESCAPE);
            escaped.push_back(byte ^ ESCAPE_XOR);
        } else {
            escaped.push_back(byte);
        }
    }
    return escaped;
}

auto hdlc::unescape_data(const hdlc_payload_t& payload) -> hdlc_payload_t {
    hdlc_payload_t unescaped;
    unescaped.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] != ESCAPE) {
            unescaped.push_back(data[i]);
            continue;
        }

        if (i + 1 >= data.size()) {
            log::error("[HDLC] Invalid escape sequence. Last byte is ESCAPE {}", ESCAPE);
            unescaped.clear();
            return unescaped;
        }

        unescaped.push_back(data[++i] ^ ESCAPE_XOR);
    }
    return unescaped;
}

} // namespace kaonic::protocols
