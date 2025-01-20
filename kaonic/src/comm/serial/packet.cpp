#include "kaonic/comm/serial/packet.hpp"

#include "kaonic/common/logging.hpp"

namespace kaonic::comm::serial {

enum class packet_type : uint16_t {
    unknown = 0,
    config = 1,
    transmit = 2,
    receive = 3,
};

auto packet::decode(const buffer_t& buffer, payload_t& payload) noexcept -> void {
    if (buffer.size() < 2) {
        payload = error_payload {};
        return;
    }

    switch (buffer[0]) {
        case static_cast<uint8_t>(packet_type::config): {
            ConfigurationRequest config;
            if (!config.ParseFromArray(buffer.data() + 1, buffer.size() - 1)) {
                log::warn("[Serial Service] TX failed: unable to parse config packet");
                payload = error_payload {};
                break;
            }
            payload = config;
            break;
        }
        case static_cast<uint8_t>(packet_type::transmit): {
            TransmitRequest request;
            if (!request.ParseFromArray(buffer.data() + 1, buffer.size() - 1)) {
                log::warn("[Serial Service] TX failed: unable to parse frame packet");
                payload = error_payload {};
                break;
            }
            payload = request;
            break;
        }
        case static_cast<uint8_t>(packet_type::receive): {
            ReceiveResponse response;
            if (!response.ParseFromArray(buffer.data() + 1, buffer.size() - 1)) {
                log::warn("[Serial Service] RX failed: unable to parse frame packet");
                payload = error_payload {};
                break;
            }
            payload = response;
            break;
        }
        default: {
            payload = error_payload {};
            break;
        }
    }
}

auto serial::packet::encode(const payload_t& payload, buffer_t& buffer) noexcept -> void {
    std::visit(
        [&buffer](auto&& payload) {
            using T = std::decay_t<decltype(payload)>;

            size_t size = 0;
            buffer.clear();
            if constexpr (std::is_same_v<T, ConfigurationRequest>) {
                size = payload.ByteSizeLong();
                buffer.resize(size + 1);
                buffer[0] = static_cast<uint8_t>(packet_type::config);
                payload.SerializeToArray(buffer.data() + 1, static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, TransmitRequest>) {
                size = payload.ByteSizeLong();
                buffer.resize(size + 1);
                buffer[0] = static_cast<uint8_t>(packet_type::transmit);
                payload.SerializeToArray(buffer.data() + 1, static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, ReceiveResponse>) {
                size = payload.ByteSizeLong();
                buffer.resize(size + 1);
                buffer[0] = static_cast<uint8_t>(packet_type::receive);
                payload.SerializeToArray(buffer.data() + 1, static_cast<int>(size));
            }
            if constexpr (std::is_same_v<T, error_payload>) {
                buffer.push_back(static_cast<uint8_t>(packet_type::unknown));
            }
        },
        payload);
}

} // namespace kaonic::comm::serial
